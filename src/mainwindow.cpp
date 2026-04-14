#include "mainwindow.h"

#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPalette>
#include <QShortcut>
#include <QStatusBar>
#include <QTextEdit>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(tr("京东读书"));
    resize(1280, 800);

    m_bm = new BookmarkManager(this);

    setupUi();
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    setupShortcuts();
    applySystemTheme();
}

// ── UI setup ──────────────────────────────────────────────────────────────────

void MainWindow::setupUi() {
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);
    setCentralWidget(m_splitter);

    m_shelf  = new BookshelfPanel(m_bm, this);
    m_reader = new ReaderView(this);

    m_splitter->addWidget(m_shelf);
    m_splitter->addWidget(m_reader);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({220, 1060});

    connect(m_reader, &ReaderView::bookshelfDataReady,
            m_shelf,  &BookshelfPanel::onBookshelfDataReady);
    connect(m_reader, &ReaderView::readerStateChanged,
            this,     &MainWindow::onReaderStateChanged);
    connect(m_shelf,  &BookshelfPanel::openBookRequested,
            this,     &MainWindow::onOpenBookRequested);
    connect(m_shelf,  &BookshelfPanel::openBookmarkRequested,
            this,     &MainWindow::onOpenBookmarkRequested);
    connect(m_reader, &ReaderView::bookIdCaptured,
            m_shelf,  &BookshelfPanel::updateBookId);
    connect(m_shelf,  &BookshelfPanel::openBookByTitleRequested,
            m_reader, &ReaderView::openBookByTitle);
}

void MainWindow::setupMenuBar() {
    // ── 文件 ──
    QMenu *fileMenu = menuBar()->addMenu(tr("文件(&F)"));
    fileMenu->addAction(tr("主页"), this, [this] {
        m_reader->load(QUrl("https://ebooks.jd.com/"));
    });
    fileMenu->addAction(tr("书架"), this, [this] {
        m_reader->load(QUrl("https://ebooks.jd.com/bookshelf"));
    });
    fileMenu->addSeparator();
    fileMenu->addAction(tr("退出"), qApp, &QApplication::quit);

    // ── 书签 ──
    QMenu *bmMenu = menuBar()->addMenu(tr("书签(&B)"));
    bmMenu->addAction(tr("添加书签\tCtrl+D"), this, &MainWindow::saveBookmark);
    bmMenu->addAction(tr("添加笔记\tCtrl+N"), this, &MainWindow::addNote);

    // ── 开发者 ──
    QMenu *devMenu = menuBar()->addMenu(tr("开发者(&D)"));
    devMenu->addAction(tr("导出书架 DOM（终端）"), this, [this] {
        m_reader->runDomDump();
    });
    devMenu->addAction(tr("刷新书架"), this, [this] {
        m_reader->triggerBookshelfScrape();
    });
    devMenu->addAction(tr("DevTools (localhost:9222)"), this, [this] {
        QDesktopServices::openUrl(QUrl("http://localhost:9222"));
    });

    // ── 视图 ──
    QMenu *viewMenu = menuBar()->addMenu(tr("视图(&V)"));
    viewMenu->addAction(tr("切换侧边栏\tCtrl+\\"), this, &MainWindow::toggleSidebar);
    viewMenu->addAction(tr("阅读模式\tF11"),        this, &MainWindow::toggleReadingMode);
    viewMenu->addSeparator();
    viewMenu->addAction(tr("放大"),  this, [this] {
        m_reader->webView()->setZoomFactor(m_reader->webView()->zoomFactor() + 0.1);
    });
    viewMenu->addAction(tr("缩小"),  this, [this] {
        m_reader->webView()->setZoomFactor(m_reader->webView()->zoomFactor() - 0.1);
    });
    viewMenu->addAction(tr("重置缩放"), this, [this] {
        m_reader->webView()->setZoomFactor(1.0);
    });
}

void MainWindow::setupToolBar() {
    m_toolbar = addToolBar(tr("导航"));
    m_toolbar->setMovable(false);

    QAction *back = m_toolbar->addAction(tr("◀"), this, [this] {
        m_reader->webView()->back();
    });
    back->setToolTip(tr("后退"));

    QAction *fwd = m_toolbar->addAction(tr("▶"), this, [this] {
        m_reader->webView()->forward();
    });
    fwd->setToolTip(tr("前进"));

    QAction *reload = m_toolbar->addAction(tr("↺"), this, [this] {
        m_reader->webView()->reload();
    });
    reload->setToolTip(tr("刷新"));

    m_toolbar->addSeparator();

    QAction *home = m_toolbar->addAction(tr("主页"), this, [this] {
        m_reader->load(QUrl("https://ebooks.jd.com/"));
    });
    home->setToolTip(tr("京东读书主页"));

    QAction *shelf = m_toolbar->addAction(tr("我的书架"), this, [this] {
        m_reader->load(QUrl("https://ebooks.jd.com/bookshelf"));
    });
    shelf->setToolTip(tr("前往书架"));

    m_toolbar->addSeparator();

    m_toolbar->addAction(tr("书签 Ctrl+D"), this, &MainWindow::saveBookmark);
    m_toolbar->addAction(tr("笔记 Ctrl+N"), this, &MainWindow::addNote);

    m_toolbar->addSeparator();
    m_toolbar->addAction(tr("阅读模式 F11"), this, &MainWindow::toggleReadingMode);
}

void MainWindow::setupStatusBar() {
    m_statusLabel = new QLabel(tr("欢迎使用京东读书"), this);
    statusBar()->addWidget(m_statusLabel, 1);
}

void MainWindow::setupShortcuts() {
    new QShortcut(QKeySequence("Ctrl+D"),    this, this, &MainWindow::saveBookmark);
    new QShortcut(QKeySequence("Ctrl+N"),    this, this, &MainWindow::addNote);
    new QShortcut(QKeySequence("Ctrl+\\"),   this, this, &MainWindow::toggleSidebar);
    new QShortcut(QKeySequence("F11"),       this, this, &MainWindow::toggleReadingMode);
    new QShortcut(QKeySequence("Ctrl+="),    this, [this] {
        m_reader->webView()->setZoomFactor(m_reader->webView()->zoomFactor() + 0.1);
    });
    new QShortcut(QKeySequence("Ctrl+-"),    this, [this] {
        m_reader->webView()->setZoomFactor(m_reader->webView()->zoomFactor() - 0.1);
    });
    new QShortcut(QKeySequence("Ctrl+0"),    this, [this] {
        m_reader->webView()->setZoomFactor(1.0);
    });
    new QShortcut(QKeySequence("Alt+Left"), this, [this] {
        m_reader->webView()->back();
    });
    new QShortcut(QKeySequence("Alt+Right"), this, [this] {
        m_reader->webView()->forward();
    });
}

void MainWindow::applySystemTheme() {
    const QPalette pal = QApplication::palette();
    m_darkMode = pal.color(QPalette::Window).lightness() < 128;
    m_reader->setDarkMode(m_darkMode);
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void MainWindow::changeEvent(QEvent *event) {
    if (event->type() == QEvent::PaletteChange) {
        applySystemTheme();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::onReaderStateChanged(const QString &bookId,
                                       const QString &bookTitle,
                                       const QString & /*chapterId*/,
                                       const QString &chapterTitle) {
    if (!bookId.isEmpty()) {
        m_bm->updateLastRead(bookId);
    }
    if (bookTitle.isEmpty() && chapterTitle.isEmpty()) {
        m_statusLabel->setText(tr("浏览中..."));
    } else if (chapterTitle.isEmpty()) {
        m_statusLabel->setText(bookTitle);
    } else {
        m_statusLabel->setText(QString("%1  ·  %2").arg(bookTitle, chapterTitle));
    }
}

void MainWindow::onOpenBookRequested(const QString &bookId) {
    m_reader->openBook(bookId);
}

void MainWindow::onOpenBookmarkRequested(const Bookmark &bm) {
    // Navigate to the book; chapter restoration via URL fragment is best-effort
    if (!bm.bookId.isEmpty()) {
        QString url = QString("https://ebooks.jd.com/reader/%1").arg(bm.bookId);
        if (!bm.chapterId.isEmpty()) {
            url += QString("#%1").arg(bm.chapterId);
        }
        m_reader->load(QUrl(url));
    }
}

void MainWindow::saveBookmark() {
    const QString bookId       = m_reader->currentBookId();
    const QString bookTitle    = m_reader->currentBookTitle();
    const QString chapterId    = m_reader->currentChapterId();
    const QString chapterTitle = m_reader->currentChapterTitle();

    if (bookId.isEmpty() && bookTitle.isEmpty()) {
        statusBar()->showMessage(tr("当前页面无法添加书签（未识别到书籍信息）"), 3000);
        return;
    }

    if (m_bm->addBookmark(bookId, bookTitle, chapterId, chapterTitle)) {
        m_shelf->refreshBookmarks();
        const QString msg = chapterTitle.isEmpty()
                                ? tr("已添加书签：%1").arg(bookTitle)
                                : tr("已添加书签：%1 · %2").arg(bookTitle, chapterTitle);
        statusBar()->showMessage(msg, 3000);
    }
}

void MainWindow::addNote() {
    const QString bookId       = m_reader->currentBookId();
    const QString bookTitle    = m_reader->currentBookTitle();
    const QString chapterId    = m_reader->currentChapterId();
    const QString chapterTitle = m_reader->currentChapterTitle();

    auto *dlg = new QDialog(this);
    dlg->setWindowTitle(chapterTitle.isEmpty()
                            ? tr("笔记 — %1").arg(bookTitle)
                            : tr("笔记 — %1 · %2").arg(bookTitle, chapterTitle));
    dlg->resize(480, 300);

    auto *layout = new QVBoxLayout(dlg);
    auto *label  = new QLabel(
        chapterTitle.isEmpty() ? bookTitle
                               : QString("%1 · %2").arg(bookTitle, chapterTitle),
        dlg);
    QFont lf = label->font();
    lf.setBold(true);
    label->setFont(lf);

    auto *edit = new QTextEdit(dlg);
    edit->setPlaceholderText(tr("在此输入笔记..."));

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);

    layout->addWidget(label);
    layout->addWidget(edit);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    if (dlg->exec() == QDialog::Accepted) {
        const QString content = edit->toPlainText().trimmed();
        if (!content.isEmpty()) {
            m_bm->addNote(bookId, bookTitle, chapterId, chapterTitle, content);
            statusBar()->showMessage(tr("笔记已保存"), 2000);
        }
    }
    dlg->deleteLater();
}

void MainWindow::toggleSidebar() {
    m_shelf->setVisible(!m_shelf->isVisible());
}

void MainWindow::toggleReadingMode() {
    m_readingMode = !m_readingMode;
    m_shelf->setVisible(!m_readingMode);
    m_toolbar->setVisible(!m_readingMode);
    menuBar()->setVisible(!m_readingMode);
    statusBar()->setVisible(!m_readingMode);
}
