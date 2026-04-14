/**
 * inject.js — 注入到每个 JD 读书页面
 *
 * 书架 DOM 结构（来自实测）：
 *   .book-card-h          → 每本书的容器
 *   .book-img img         → 封面图片
 *   .book-name            → 书名
 *   .book-author          → 作者
 *
 * bookId 不在 HTML 属性里，采用两阶段策略：
 *   阶段 1：从 Vue 3 组件内部状态提取（快速，成功率高）
 *   阶段 2：在用户点击书目时拦截 URL 变化，回报 ID（保底）
 */
(function () {
    'use strict';

    // ── URL 模式 ──────────────────────────────────────────────────────────────

    const SHELF_URL_PATTERNS  = ['/bookshelf'];
    const READER_URL_PATTERNS = ['/reader'];

    // 阅读页章节/书名选择器（根据实测调整）
    const READER_BOOK_TITLE_SELECTORS = [
        '[class*="bookTitle"]', '[class*="book-title"]',
        '[class*="bookName"]',  'h1',
    ];
    const READER_CHAPTER_SELECTORS = [
        '[class*="chapterTitle"]', '[class*="chapter-title"]',
        '[class*="chapterName"]',  '[class*="currentChapter"]',
        'h2',
    ];

    // ── 内部状态 ──────────────────────────────────────────────────────────────

    let bridge          = null;
    let lastUrl         = location.href;
    let shelfObserver   = null;
    let chapterObserver = null;

    // ── 工具：从 URL 提取 bookId ──────────────────────────────────────────────

    function extractBookId(url) {
        if (!url) return '';
        const patterns = [
            /\/reader\/(\d+)/i,
            /[?&]bookId=(\d+)/i,
            /\/(\d{7,})/,
        ];
        for (var i = 0; i < patterns.length; i++) {
            var m = url.match(patterns[i]);
            if (m) return m[1];
        }
        return '';
    }

    // ── 工具：从 Vue 3 组件实例提取 bookId ───────────────────────────────────

    function getIdFromObj(obj) {
        if (!obj || typeof obj !== 'object') return '';
        var id = obj.bookId || obj.id || obj.sku || obj.bookSku || obj.volumeId;
        if (id) return String(id);
        // 嵌套的 book/item 对象
        var nested = obj.book || obj.bookInfo || obj.item || obj.detail || obj.data;
        if (nested && typeof nested === 'object') {
            id = nested.bookId || nested.id || nested.sku || nested.bookSku;
            if (id) return String(id);
        }
        return '';
    }

    function getBookIdFromVue(el) {
        // 找 Vue 3 在 DOM 元素上挂的实例 key（通常是 __vueParentComponent）
        var vueKey = '';
        var keys = Object.keys(el);
        for (var i = 0; i < keys.length; i++) {
            if (keys[i].startsWith('__vue')) { vueKey = keys[i]; break; }
        }
        if (!vueKey) return '';

        var instance = el[vueKey];
        if (!instance) return '';

        // 尝试 props / setupState / ctx / data
        var sources = [
            instance.props,
            instance.setupState,
            instance.ctx,
            instance.data,
        ];
        for (var j = 0; j < sources.length; j++) {
            var id = getIdFromObj(sources[j]);
            if (id) return id;
        }

        // 尝试父组件（book 列表往往在父组件持有 list，子组件收 item）
        var parent = instance.parent;
        if (parent && parent.props) {
            var list = parent.props.bookList || parent.props.list ||
                       parent.props.books    || parent.props.data;
            if (Array.isArray(list)) {
                // 拿当前 el 在父容器里的 index，对应 list[index]
                var siblings = el.parentElement
                    ? Array.from(el.parentElement.querySelectorAll(':scope > .book-card-h'))
                    : [];
                var idx = siblings.indexOf(el);
                if (idx >= 0 && list[idx]) {
                    var id2 = getIdFromObj(list[idx]);
                    if (id2) return id2;
                }
            }
        }
        return '';
    }

    // ── 书架抓取 ──────────────────────────────────────────────────────────────

    function scrapeBookshelf(br) {
        br = br || bridge;
        if (!br) return;

        var bookEls = Array.from(document.querySelectorAll('.book-card-h'));
        if (bookEls.length === 0) return;

        var books = [];
        bookEls.forEach(function (el) {
            var titleEl  = el.querySelector('.book-name');
            var authorEl = el.querySelector('.book-author');
            var imgEl    = el.querySelector('.book-img img');

            var title    = titleEl  ? titleEl.textContent.trim()  : '';
            var author   = authorEl ? authorEl.textContent.trim() : '';
            var coverUrl = imgEl    ? (imgEl.src || imgEl.dataset.src || '') : '';

            if (!title) return;

            // 先尝试 Vue 内部状态拿 bookId
            var bookId = getBookIdFromVue(el);

            books.push({ bookId: bookId, title: title, author: author, coverUrl: coverUrl });

            // 如果没拿到 bookId，监听点击来后续捕获
            if (!bookId) {
                attachClickCapture(el, title, br);
            }
        });

        if (books.length > 0) {
            br.onBookshelfLoaded(JSON.stringify(books));
        }
    }

    // 暴露到全局，供 C++ triggerBookshelfScrape() 调用
    window._jdrScrapeBookshelf = scrapeBookshelf;

    // ── 点击捕获：用户点书后等待 URL 变化，提取 bookId ───────────────────────

    function attachClickCapture(el, title, br) {
        el.addEventListener('click', function onClick() {
            el.removeEventListener('click', onClick);
            var urlBefore = location.href;
            setTimeout(function () {
                var newUrl = location.href;
                if (newUrl !== urlBefore) {
                    var bookId = extractBookId(newUrl);
                    if (bookId) {
                        br.onBookIdCaptured(bookId, title);
                    }
                }
            }, 800);
        });
    }

    // ── 书架观察者 ────────────────────────────────────────────────────────────

    function startShelfObserver() {
        if (shelfObserver) shelfObserver.disconnect();

        var debounceTimer = null;
        var fired = false;

        shelfObserver = new MutationObserver(function () {
            clearTimeout(debounceTimer);
            debounceTimer = setTimeout(function () {
                scrapeBookshelf();
                fired = true;
            }, fired ? 2500 : 800);
        });
        shelfObserver.observe(document.body, { childList: true, subtree: true });

        setTimeout(function () {
            if (shelfObserver) { shelfObserver.disconnect(); shelfObserver = null; }
        }, 20000);
    }

    // ── 阅读状态追踪 ──────────────────────────────────────────────────────────

    function queryFirst(selectors, root) {
        root = root || document;
        for (var i = 0; i < selectors.length; i++) {
            try {
                var el = root.querySelector(selectors[i]);
                if (el) return el;
            } catch (e) {}
        }
        return null;
    }

    function scrapeReaderState() {
        if (!bridge) return;

        var bookId = extractBookId(location.href);

        var titleEl   = queryFirst(READER_BOOK_TITLE_SELECTORS);
        var chapterEl = queryFirst(READER_CHAPTER_SELECTORS);

        // Fallback: 从 <title> 解析 "书名 - 章节 - 京东读书"
        var parts = document.title.split(/\s*[-—|·]\s*/);
        var bookTitle    = parts[0] ? parts[0].trim() : '';
        var chapterTitle = parts[1] ? parts[1].trim() : '';

        if (titleEl)   bookTitle    = titleEl.textContent.trim()   || bookTitle;
        if (chapterEl) chapterTitle = chapterEl.textContent.trim() || chapterTitle;

        var chapterId =
            location.hash.replace('#', '') ||
            (chapterEl ? (chapterEl.dataset.chapterid || chapterEl.id || '') : '');

        bridge.onReaderStateChanged(bookId, bookTitle, chapterId, chapterTitle);
    }

    function startChapterObserver() {
        if (chapterObserver) chapterObserver.disconnect();
        var lastChapter = '';
        var debounceTimer = null;

        chapterObserver = new MutationObserver(function () {
            clearTimeout(debounceTimer);
            debounceTimer = setTimeout(function () {
                if (location.href !== lastUrl) {
                    lastUrl = location.href;
                    scrapeReaderState();
                    return;
                }
                var el = queryFirst(READER_CHAPTER_SELECTORS);
                var ch = el ? el.textContent.trim() : '';
                if (ch && ch !== lastChapter) {
                    lastChapter = ch;
                    scrapeReaderState();
                }
            }, 400);
        });
        chapterObserver.observe(document.body, { childList: true, subtree: true });
    }

    // ── 页面类型分发 ──────────────────────────────────────────────────────────

    function getPageType() {
        var path = location.pathname;
        for (var i = 0; i < READER_URL_PATTERNS.length; i++) {
            if (path.includes(READER_URL_PATTERNS[i])) return 'reader';
        }
        for (var j = 0; j < SHELF_URL_PATTERNS.length; j++) {
            if (path.includes(SHELF_URL_PATTERNS[j])) return 'shelf';
        }
        return 'other';
    }

    function onPageReady() {
        var type = getPageType();
        if (type === 'shelf') {
            setTimeout(scrapeBookshelf, 1200);
            startShelfObserver();
        } else if (type === 'reader') {
            setTimeout(scrapeReaderState, 600);
            startChapterObserver();
        }
    }

    // ── SPA 导航拦截 ──────────────────────────────────────────────────────────

    var origPushState    = history.pushState.bind(history);
    var origReplaceState = history.replaceState.bind(history);

    history.pushState = function () {
        origPushState.apply(history, arguments);
        window.dispatchEvent(new Event('jdr:navigate'));
    };
    history.replaceState = function () {
        origReplaceState.apply(history, arguments);
        window.dispatchEvent(new Event('jdr:navigate'));
    };
    window.addEventListener('popstate', function () {
        window.dispatchEvent(new Event('jdr:navigate'));
    });
    window.addEventListener('jdr:navigate', function () {
        lastUrl = location.href;
        if (shelfObserver)   { shelfObserver.disconnect();   shelfObserver   = null; }
        if (chapterObserver) { chapterObserver.disconnect(); chapterObserver = null; }
        setTimeout(onPageReady, 500);
    });

    // ── 初始化 QWebChannel ────────────────────────────────────────────────────

    function initBridge(attempt) {
        attempt = attempt || 0;
        if (attempt > 50) { console.warn('[JDReader] WebChannel timeout'); return; }
        if (typeof QWebChannel === 'undefined' ||
            typeof qt === 'undefined' || !qt.webChannelTransport) {
            setTimeout(function () { initBridge(attempt + 1); }, 150);
            return;
        }
        new QWebChannel(qt.webChannelTransport, function (channel) {
            bridge = channel.objects.bridge;
            onPageReady();
        });
    }

    initBridge();
})();
