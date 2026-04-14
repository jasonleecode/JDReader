#include "ReaderView.h"

#include <QFile>
#include <QTimer>
#include <QVBoxLayout>
#include <QDebug>
#include <QStandardPaths>

#include <QtWebEngineWidgets/QWebEngineView>
#include <QtWebEngineCore/QWebEngineProfile>
#include <QtWebEngineCore/QWebEnginePage>
#include <QtWebEngineCore/QWebEngineScript>
#include <QtWebEngineCore/QWebEngineScriptCollection>
#include <QtWebEngineCore/QWebEngineSettings>
#include <QtWebChannel/QWebChannel>

ReaderView::ReaderView(QWidget *parent) : QWidget(parent) {
    setupProfile();

    m_view = new QWebEngineView(this);
    m_view->setPage(m_page);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);

    connect(m_view, &QWebEngineView::loadFinished,
            this,   &ReaderView::onLoadFinished);
    connect(m_view, &QWebEngineView::loadStarted,
            this,   &ReaderView::loadingStarted);

    load(QUrl(JD_HOME));
}

void ReaderView::setupProfile() {
    // Persistent profile: cookies / cache survive restarts
    m_profile = new QWebEngineProfile("JDReader", this);

    // Use a real Chrome user-agent so JD doesn't refuse the WebView
    m_profile->setHttpUserAgent(
        "Mozilla/5.0 (X11; Linux x86_64) "
        "AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/120.0.0.0 Safari/537.36");

    m_page = new QWebEnginePage(m_profile, this);

    // Allow JS, local storage, etc.
    m_page->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    m_page->settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
    m_page->settings()->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled, true);

    // WebChannel bridge
    m_channel = new QWebChannel(this);
    m_bridge  = new WebBridge(this);
    m_channel->registerObject("bridge", m_bridge);
    m_page->setWebChannel(m_channel);

    connect(m_bridge, &WebBridge::bookshelfLoaded,
            this,     &ReaderView::onBridgeBookshelfLoaded);
    connect(m_bridge, &WebBridge::readerStateChanged,
            this,     &ReaderView::onBridgeReaderStateChanged);
    connect(m_bridge, &WebBridge::bookIdCaptured,
            this,     &ReaderView::bookIdCaptured);

    injectScripts();
}

void ReaderView::injectScripts() {
    // Helper lambda: inject a script file from Qt resources
    auto inject = [this](const QString &resPath, const QString &name,
                          QWebEngineScript::InjectionPoint point,
                          QWebEngineScript::ScriptWorldId world) {
        QFile f(resPath);
        if (!f.open(QIODevice::ReadOnly)) {
            qWarning() << "Cannot open resource:" << resPath;
            return;
        }
        QWebEngineScript s;
        s.setName(name);
        s.setSourceCode(QString::fromUtf8(f.readAll()));
        s.setInjectionPoint(point);
        s.setWorldId(world);
        s.setRunsOnSubFrames(false);
        m_page->scripts().insert(s);
    };

    // 1. qwebchannel.js  – must run before our inject.js
    inject(":/resources/qwebchannel.js", "qwebchannel.js",
           QWebEngineScript::DocumentCreation, QWebEngineScript::MainWorld);

    // 2. inject.css  – DocumentReady so the DOM exists
    {
        QFile f(":/resources/inject.css");
        if (f.open(QIODevice::ReadOnly)) {
            QString css = QString::fromUtf8(f.readAll())
                              .replace("\\", "\\\\")
                              .replace("`",  "\\`");
            QString js = QString(
                "(function(){"
                "  const s=document.createElement('style');"
                "  s.id='jdreader-style';"
                "  s.textContent=`%1`;"
                "  document.head.appendChild(s);"
                "})();").arg(css);
            QWebEngineScript s;
            s.setName("inject.css");
            s.setSourceCode(js);
            s.setInjectionPoint(QWebEngineScript::DocumentReady);
            s.setWorldId(QWebEngineScript::MainWorld);
            s.setRunsOnSubFrames(false);
            m_page->scripts().insert(s);
        }
    }

    // 3. inject.js  – DocumentReady, after qwebchannel.js is available
    inject(":/resources/inject.js", "inject.js",
           QWebEngineScript::DocumentReady, QWebEngineScript::MainWorld);
}

void ReaderView::load(const QUrl &url) {
    m_view->load(url);
}

void ReaderView::openBook(const QString &bookId) {
    const QUrl url = QUrl(QString("https://ebooks.jd.com/reader/%1").arg(bookId));
    load(url);
}

void ReaderView::openBookByTitle(const QString &title) {
    if (title.isEmpty()) return;
    m_pendingBookTitle = title;

    // If already on the bookshelf page, auto-click immediately;
    // otherwise navigate there first — onLoadFinished will auto-click.
    const QString path = m_page->url().path();
    if (path.contains("/bookshelf")) {
        autoClickBookByTitle(title);
    } else {
        load(QUrl("https://ebooks.jd.com/bookshelf"));
    }
}

// Private helper — runs JS that finds the book card matching [title] and clicks it.
// Called either immediately (already on bookshelf) or from onLoadFinished.
void ReaderView::autoClickBookByTitle(const QString &title) {
    // Escape single quotes in title for safe JS string literal
    const QString safe = QString(title).replace("'", "\\'");
    const QString js = QString(R"JS(
(function() {
    var cards = document.querySelectorAll('.book-card-h');
    for (var i = 0; i < cards.length; i++) {
        var nameEl = cards[i].querySelector('.book-name');
        if (nameEl && nameEl.textContent.trim() === '%1') {
            cards[i].click();
            return true;
        }
    }
    // Not rendered yet — retry once after a short delay
    setTimeout(function() {
        var cards2 = document.querySelectorAll('.book-card-h');
        for (var j = 0; j < cards2.length; j++) {
            var n = cards2[j].querySelector('.book-name');
            if (n && n.textContent.trim() === '%1') { cards2[j].click(); return; }
        }
    }, 1200);
})();
)JS").arg(safe);
    m_page->runJavaScript(js);
}

void ReaderView::runDomDump() {
    // Walk the top 3 levels of the DOM tree, collect element tag+class info,
    // then send it to C++ via bridge.onDomDump() so it appears on stderr.
    const QString js = R"JS(
(function() {
    if (typeof qt === 'undefined' || !qt.webChannelTransport) {
        console.warn('[JDReader] WebChannel not ready for DOM dump');
        return;
    }
    new QWebChannel(qt.webChannelTransport, function(ch) {
        var lines = ['URL: ' + location.href, ''];
        function walk(el, depth) {
            if (depth > 3) return;
            var indent = '  '.repeat(depth);
            var cls = el.className && typeof el.className === 'string'
                        ? '.' + el.className.trim().replace(/\s+/g, '.') : '';
            var id  = el.id ? '#' + el.id : '';
            var datab = el.dataset && el.dataset.bookid ? ' [data-bookid=' + el.dataset.bookid + ']' : '';
            var text = '';
            if (el.childElementCount === 0 && el.textContent) {
                text = ' "' + el.textContent.trim().slice(0,40) + '"';
            }
            lines.push(indent + el.tagName + id + cls + datab + text);
            Array.from(el.children).slice(0, 20).forEach(function(c) { walk(c, depth+1); });
        }
        walk(document.body, 0);
        ch.objects.bridge.onDomDump(lines.join('\n'));
    });
})();
)JS";
    m_page->runJavaScript(js);
}

void ReaderView::triggerBookshelfScrape() {
    const QString js = R"JS(
(function() {
    if (typeof qt === 'undefined' || !qt.webChannelTransport) return;
    new QWebChannel(qt.webChannelTransport, function(ch) {
        // Re-run the bookshelf scrape from inject.js globals if available,
        // otherwise fall back to a generic link+image scan.
        if (typeof window._jdrScrapeBookshelf === 'function') {
            window._jdrScrapeBookshelf(ch.objects.bridge);
            return;
        }
        // Generic fallback: find all <a> tags that contain an <img>
        var results = [];
        document.querySelectorAll('a[href]').forEach(function(a) {
            var img = a.querySelector('img');
            if (!img) return;
            var href = a.href || '';
            var bookIdMatch = href.match(/[\/=](\d{7,})/);
            if (!bookIdMatch) return;
            var titleEl = a.querySelector('[class*="title"],[class*="name"],p,span');
            var title = titleEl ? titleEl.textContent.trim() : a.getAttribute('title') || '';
            if (!title && !bookIdMatch[1]) return;
            results.push({bookId: bookIdMatch[1], title: title, coverUrl: img.src || ''});
        });
        if (results.length > 0)
            ch.objects.bridge.onBookshelfLoaded(JSON.stringify(results));
    });
})();
)JS";
    m_page->runJavaScript(js);
}

void ReaderView::setDarkMode(bool dark) {
    if (m_darkMode == dark) return;
    m_darkMode = dark;
    const QString js = QString(
        "document.documentElement.setAttribute('data-jdreader-theme','%1');")
        .arg(dark ? "dark" : "light");
    m_page->runJavaScript(js);
}

void ReaderView::onLoadFinished(bool ok) {
    if (!ok) {
        qWarning() << "Page load failed";
    }
    setDarkMode(m_darkMode);

    // If we navigated to bookshelf to auto-click a book by title, do it now.
    if (!m_pendingBookTitle.isEmpty()) {
        const QString path = m_page->url().path();
        if (path.contains("/bookshelf")) {
            // Bookshelf may still be rendering; give React/Vue a moment.
            const QString title = m_pendingBookTitle;
            m_pendingBookTitle.clear();
            QTimer::singleShot(1000, this, [this, title] { autoClickBookByTitle(title); });
        }
    }

    emit loadingFinished();
}

void ReaderView::onBridgeBookshelfLoaded(const QString &json) {
    emit bookshelfDataReady(json);
}

void ReaderView::onBridgeReaderStateChanged(const QString &bookId,
                                             const QString &bookTitle,
                                             const QString &chapterId,
                                             const QString &chapterTitle) {
    m_bookId       = bookId;
    m_bookTitle    = bookTitle;
    m_chapterId    = chapterId;
    m_chapterTitle = chapterTitle;
    emit readerStateChanged(bookId, bookTitle, chapterId, chapterTitle);
}
