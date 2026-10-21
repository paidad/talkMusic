/* 网页只负责编辑与上报带版本的 Markdown；文件持久化由 C++ 的 NoteDocument 统一管理。 */
(() => {
  'use strict';
  // generation 区分内容重载，revision 区分本轮输入；original 保证只查看时不改写原文。
  let host, editor, generation = 0, revision = 0, original = '', modified = false;
  // muted 抑制程序初始化上报；composing 保护中文组合输入；savedRange 供菜单恢复选区。
  let muted = false, composing = false, readonly = false, savedRange = null, decorationQueued = false;
  const menu = document.getElementById('menu');
  const mainMenu = document.getElementById('menu-main');
  const submenus = Array.from(menu.querySelectorAll('.submenu'));
  let menuSession = 0, submenuCloseTimer = 0;
  const element = () => editor.vditor.ir.element;
  const selection = () => window.getSelection();
  const remember = () => { const s = selection(); if (s.rangeCount && element().contains(s.anchorNode)) savedRange = s.getRangeAt(0).cloneRange(); };
  // 菜单会转移焦点，只恢复仍挂在当前 DOM 上的选区，并重新展开选中的代码源码。
  const restore = () => {
    editor.focus();
    if (savedRange && savedRange.startContainer.isConnected) {
      const s = selection(); s.removeAllRanges(); s.addRange(savedRange);
      const node = savedRange.startContainer.nodeType === 1 ? savedRange.startContainer : savedRange.startContainer.parentElement;
      const code = node.closest('[data-type="code-block"]');
      if (code && !readonly) code.classList.add('vditor-ir__node--expand');
    }
  };
  // 不把载入、只读变化或尚未确认的输入法文本报告为可保存的用户编辑。
  const report = () => {
    if (muted || composing || readonly) return;
    modified = true; const text = editor.getValue(); ++revision;
    host.changed(generation, revision, text); scheduleDecoration();
  };
  // 代码块装饰不改正文：行号写入 data 属性，复制按钮放在独立浮层。
  function decorate() {
    decorationQueued = false;
    if (!editor || !editor.vditor.ir || composing) return;
    const tools = document.getElementById('code-tools'); tools.replaceChildren();
    element().querySelectorAll('[data-type="code-block"]').forEach(block => {
      const source = block.querySelector('.vditor-ir__marker--pre > code');
      const rendered = block.querySelector('.vditor-ir__preview');
      // 预览只是展示表面，点击由 Vditor 转入源码；禁止浏览器直接编辑第二份渲染内容。
      if (rendered) rendered.setAttribute('contenteditable', 'false');
      const info = block.querySelector('[data-type="code-block-info"]');
      block.dataset.language = info ? info.textContent.replace(/\u200b/g, '').trim() || '纯文本' : '纯文本';
      if (source) {
        const count = source.textContent.replace(/\n$/, '').split('\n').length;
        source.parentElement.dataset.lines = Array.from({length:count}, (_, i) => String(i + 1)).join('\n');
        const preview = block.querySelector('.vditor-ir__preview code');
        if (preview) preview.dataset.lines = source.parentElement.dataset.lines;
      }
      const rect = block.getBoundingClientRect();
      if (rect.bottom < 0 || rect.top > innerHeight) return;
      const button = document.createElement('button'); button.textContent = '复制'; button.title = '复制代码，不含行号';
      button.style.left = `${Math.max(0, rect.right - 64)}px`; button.style.top = `${rect.top + 5}px`;
      button.addEventListener('mousedown', e => e.preventDefault());
      button.addEventListener('click', () => {host.clipboardWrite(source ? source.textContent.replace(/\n$/, '') : ''); button.textContent = '已复制';}); tools.append(button);
    });
  }
  // 同一帧只安排一次装饰更新，合并 DOM、滚动和缩放触发的重复请求。
  function scheduleDecoration() { if (!decorationQueued) {decorationQueued = true; requestAnimationFrame(decorate);} }
  function toolbar(name) {
    // 使用当前固定版本编辑器自身的工具栏命令，使操作参与其撤销栈。
    const button = document.querySelector(`.vditor-toolbar button[data-type="${name}"]`);
    if (button) button.click();
  }
  function closeSubmenus() {
    clearTimeout(submenuCloseTimer);
    mainMenu.hidden = false; delete menu.dataset.compact;
    submenus.forEach(panel => {panel.hidden = true; panel.querySelector('[data-back]').hidden = true;});
    menu.querySelectorAll('[data-submenu]').forEach(button => button.setAttribute('aria-expanded', 'false'));
  }
  // 菜单会话号使异步剪贴板查询不能回写已关闭或重新打开的旧菜单。
  function closeMenu(restoreSelection = false) {
    ++menuSession; menu.hidden = true; closeSubmenus();
    if (restoreSelection && editor && !readonly) restore();
  }
  function entries(container) {
    return Array.from(container.querySelectorAll('button')).filter(button => !button.disabled && button.getClientRects().length);
  }
  function focusItem(button) {
    if (!button) return;
    button.focus({preventScroll:true}); button.scrollIntoView({block:'nearest',inline:'nearest'});
  }
  function updateMenuState() {
    const node = savedRange && (savedRange.startContainer.nodeType === 1 ? savedRange.startContainer : savedRange.startContainer.parentElement);
    const inCode = !!(node && node.closest('[data-type="code-block"]'));
    const selected = !!(savedRange && !savedRange.collapsed);
    menu.querySelectorAll('[data-command]').forEach(button => {
      const name = button.dataset.command;
      if (name === 'cut' || name === 'copy') button.disabled = !selected;
      else if (name === 'paste') return; // Clipboard availability is queried once per opening.
      else {
        const action = document.querySelector(`.vditor-toolbar button[data-type="${/^h[1-6]$/.test(name) || name === 'paragraph' ? 'headings' : name}"]`);
        button.disabled = (inCode && name !== 'undo' && name !== 'redo') || !!(action && (action.disabled || action.classList.contains('vditor-menu--disabled')));
      }
    });
    menu.querySelectorAll('[data-submenu]').forEach(button => {
      const panel = document.getElementById(button.getAttribute('aria-controls'));
      button.disabled = !Array.from(panel.querySelectorAll('[data-command]')).some(item => !item.disabled);
    });
  }
  function openSubmenu(button, keyboard = false) {
    if (button.disabled) return;
    clearTimeout(submenuCloseTimer);
    const rootRect = menu.getBoundingClientRect(), triggerRect = button.getBoundingClientRect();
    closeSubmenus();
    const panel = document.getElementById(button.getAttribute('aria-controls'));
    panel.hidden = false; panel.style.width = '';
    const width = panel.getBoundingClientRect().width, gap = 4, margin = 8;
    const rightFits = rootRect.right + gap + width <= innerWidth - margin;
    const leftFits = rootRect.left - gap - width >= margin;
    let left, top;
    if (!rightFits && !leftFits) {
      if (!keyboard) {panel.hidden = true; return;} // Enter compact menus only on click or a key.
      // A narrow writing pane cannot fit two menu columns. Replace the first
      // column temporarily, with an explicit way back instead of overlapping it.
      menu.dataset.compact = 'true'; mainMenu.hidden = true;
      panel.style.width = `${rootRect.width}px`; panel.querySelector('[data-back]').hidden = false;
      left = rootRect.left; top = rootRect.top;
    } else {
      left = rightFits ? rootRect.right + gap : rootRect.left - width - gap;
      top = triggerRect.top - 6;
    }
    // Borders can occupy fractional CSS pixels at 125%/150% scale. Integer
    // offsetHeight rounds those away and can push the lower edge out of bounds.
    const size = panel.getBoundingClientRect();
    panel.style.left = `${Math.floor(Math.max(margin, Math.min(left, innerWidth - size.width - margin)))}px`;
    panel.style.top = `${Math.floor(Math.max(margin, Math.min(top, innerHeight - size.height - margin)))}px`;
    button.setAttribute('aria-expanded', 'true');
    if (keyboard) focusItem(Array.from(panel.querySelectorAll('[data-command]')).find(item => !item.disabled));
  }
  function returnToMain(panel) {
    const trigger = menu.querySelector(`[aria-controls="${panel.id}"]`);
    closeSubmenus(); focusItem(trigger);
  }
  // 菜单打开时接管方向键与激活键，避免这些按键同时修改底下的编辑区。
  function menuKeys(event) {
    if (menu.hidden) return false;
    const focused = document.activeElement;
    const panel = focused && focused.closest('.submenu');
    const container = panel || mainMenu;
    const items = entries(container);
    if (event.ctrlKey || event.metaKey) {
      const name = {z:'undo',y:'redo',x:'cut',c:'copy',v:'paste',b:'bold',i:'italic',k:'link'}[event.key.toLowerCase()];
      if (name && !event.shiftKey) {
        const item = menu.querySelector(`[data-command="${name}"]`);
        if (item && !item.disabled) command(name);
      } else if (event.key.toLowerCase() === 's') {closeMenu(true); host.saveRequested();}
      else return false;
    } else if (event.key === 'Escape' || event.key === 'ArrowLeft') {
      if (panel) returnToMain(panel);
      else closeMenu(true);
    } else if (event.key === 'Tab') closeMenu(true);
    else if (event.key === 'ArrowRight') {
      if (focused && focused.dataset.submenu) openSubmenu(focused, true);
    } else if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
      const direction = event.key === 'ArrowDown' ? 1 : -1;
      const index = items.indexOf(focused);
      focusItem(items[(index + direction + items.length) % items.length]);
    } else if (event.key === 'Home') focusItem(items[0]);
    else if (event.key === 'End') focusItem(items[items.length - 1]);
    else if (event.key === 'Enter' || event.key === ' ') {if (focused && !focused.disabled && menu.contains(focused)) focused.click();}
    else return false;
    event.preventDefault(); event.stopImmediatePropagation(); return true;
  }
  // 所有右键命令走同一入口；执行前恢复选区，组合输入与只读状态不接受修改。
  function command(name) {
    if (readonly || composing) return;
    closeMenu(); restore();
    if (name === 'undo') editor.vditor.undo.addToUndoStack(editor.vditor);
    if (name === 'copy' || name === 'cut') {
      host.clipboardWrite(editor.getSelection());
      if (name === 'cut') { editor.deleteValue(); report(); }
    } else if (name === 'paste') {
      // 读取剪贴板是异步的，回调到达时可能已切换文档或进入只读状态。
      const version = generation;
      host.clipboardRead(text => { if (!text || generation !== version || readonly || composing) return; restore(); editor.insertMD(text); report(); });
    } else if (name === 'image') host.chooseImage();
    else if (/^h[1-6]$/.test(name)) {
      document.querySelector(`.vditor-toolbar button[data-tag="${name}"]`).click(); report();
    } else if (name === 'paragraph') {
      const range = selection().rangeCount ? selection().getRangeAt(0) : null;
      const node = range && (range.startContainer.nodeType === 1 ? range.startContainer : range.startContainer.parentElement);
      const heading = node && node.closest('h1,h2,h3,h4,h5,h6');
      if (heading) {
        // 当前标题命令可移除标题标记并重新解析该块，转回正文无需直接重写 DOM。
        const button = document.querySelector('.vditor-toolbar button[data-type="headings"]');
        button.classList.add('vditor-menu--current'); button.click(); report();
      }
    } else {toolbar(name); report();}
    remember(); scheduleDecoration();
  }
  document.querySelectorAll('#menu button').forEach(button => {
    button.addEventListener('mousedown', e => e.preventDefault());
    button.addEventListener('mouseenter', () => {
      clearTimeout(submenuCloseTimer);
      if (button.disabled) return;
      focusItem(button);
      if (button.dataset.submenu) openSubmenu(button);
      else if (mainMenu.contains(button)) closeSubmenus();
    });
    button.addEventListener('click', () => {
      if (button.disabled) return;
      if (button.dataset.submenu) openSubmenu(button, true);
      else if (button.hasAttribute('data-back')) returnToMain(button.closest('.submenu'));
      else command(button.dataset.command);
    });
  });
  menu.addEventListener('mouseleave', () => {if (!menu.dataset.compact) submenuCloseTimer = setTimeout(closeSubmenus, 240);});
  menu.addEventListener('mouseenter', () => clearTimeout(submenuCloseTimer));
  submenus.forEach(panel => panel.addEventListener('mouseenter', () => clearTimeout(submenuCloseTimer)));
  // 只在当前编辑区域显示自定义菜单，并根据真实浮点尺寸约束在窗口边缘内。
  document.addEventListener('contextmenu', e => {
    if (!editor || readonly || !element().contains(e.target)) return;
    e.preventDefault(); if (composing) return;
    closeMenu(); remember(); menu.hidden = false; updateMenuState();
    const session = menuSession;
    const paste = menu.querySelector('[data-command="paste"]'); paste.disabled = true;
    host.clipboardRead(text => {if (session === menuSession && !menu.hidden) paste.disabled = !text.length;});
    setTimeout(() => {if (session === menuSession && !menu.hidden) updateMenuState();}, 150);
    const size = menu.getBoundingClientRect();
    menu.style.left = `${Math.floor(Math.max(8, Math.min(e.clientX, innerWidth - size.width - 8)))}px`;
    menu.style.top = `${Math.floor(Math.max(8, Math.min(e.clientY, innerHeight - size.height - 8)))}px`;
    focusItem(entries(mainMenu)[0]);
  });
  document.addEventListener('mousedown', e => { if (!menu.contains(e.target)) closeMenu(); });
  document.addEventListener('keydown', menuKeys, true);
  document.addEventListener('keydown', e => {
    if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 's') {e.preventDefault(); host.saveRequested();}
  });
  document.addEventListener('selectionchange', () => { if (menu.hidden && editor) remember(); });
  window.addEventListener('resize', () => {closeMenu(); scheduleDecoration();});
  window.addEventListener('blur', () => closeMenu());
  document.addEventListener('scroll', e => {if (!menu.contains(e.target)) closeMenu(); scheduleDecoration();}, true);
  // C++ runJavaScript 的固定入口；参数使用 JSON 传递，不直接拼接用户文本。
  window.talkMusic = {
    load(config) {
      // 初始化不标脏、不报告输入；除明确重置外尽量保留编辑器已有撤销状态。
      muted = true; generation = config.generation; revision = config.revision; original = config.text;
      modified = false; savedRange = null;
      const scroll = element().scrollTop;
      editor.setValue(original, !!config.resetUndo); element().scrollTop = scroll;
      this.readOnly(config.readOnly); muted = false; scheduleDecoration(); host.loaded(generation);
    },
    readOnly(value) {readonly = value; if (value) editor.disabled(); else editor.enable(); element().querySelectorAll('input').forEach(i => {i.disabled=value;}); closeMenu(); scheduleDecoration();},
    snapshot() {
      // 最后一次 input 可能早于防抖回调；快照主动补报，但组合输入中不序列化。
      if (modified && !composing) {report(); editor.vditor.undo.addToUndoStack(editor.vditor);}
      return {generation, revision, modified, composing, text: modified ? editor.getValue() : original};
    },
    insert(data) { if (!readonly && !composing) {restore(); editor.insertMD(data.text); report(); remember();} },
    heading(ordinal) {
      const headings = element().querySelectorAll('h1,h2,h3,h4,h5,h6');
      const node = headings[ordinal]; if (!node) return;
      node.scrollIntoView({block:'start'});
      if (!readonly) {
        const range = document.createRange(); range.selectNodeContents(node); range.collapse(false);
        selection().removeAllRanges(); selection().addRange(range); remember();
      }
    },
    // 集成测试与真实菜单复用同一命令路径，避免测试绕过实际编辑逻辑。
    command
  };
  // 先取得宿主配置，再创建离线 Vditor；禁用自主缓存避免出现第二份保存权威。
  new QWebChannel(qt.webChannelTransport, channel => {
    host = channel.objects.host;
    host.configuration(config => {
      editor = new Vditor('editor', {
        cdn:'qrc:/vditor', lang:'zh_CN', i18n:window.VditorI18n, height:'100%', mode:'ir',
        cache:{enable:false}, value:'', undoDelay:120, icon:'ant',
        toolbar:['headings','bold','italic','inline-code','link','quote','list','ordered-list','check','code','line','undo','redo'],
        toolbarConfig:{hide:true}, link:{isOpen:false,click(node) {
          if (!node) return; const url=node.textContent.replace(/\u200b/g,'').trim();
          if (url.startsWith('#')) {const target=document.getElementById(decodeURIComponent(url.slice(1)));if(target)target.scrollIntoView();}
          else host.openLink(url);
        }}, image:{isPreview:false},
        preview:{maxWidth:800, actions:[], hljs:{enable:true,lineNumber:false,style:'github',defaultLang:''},
          markdown:{sanitize:true,linkBase:'talknote://images/',mathBlockPreview:false,codeBlockPreview:true,footnotes:false,toc:false},
          theme:{current:'light',path:'qrc:/vditor/dist/css/content-theme'}, render:{media:{enable:false}}},
        hint:{emoji:{},emojiPath:'qrc:/vditor/dist/images/emoji'},
        input:report,
        after() {
          // after 可能早于 editor 赋值完成；延后一轮再访问编辑器实例。
          setTimeout(() => {
            // 将项目样式放在第三方主题之后，确保暖白排版覆盖规则有效。
            document.head.appendChild(document.getElementById('talkMusicTheme'));
            talkMusic.load({...config,resetUndo:true});
            element().addEventListener('compositionstart', () => {composing = true;});
            element().addEventListener('compositionend', () => {composing = false; report();});
            // 同步保存可能先于 Vditor 的防抖回调，原生 input 先记录有未报告修改。
            element().addEventListener('input', () => {if (!muted && !readonly) modified = true;});
            // 首版不接受粘贴/拖放附件；本地图片必须经过 C++ 选择与路径验证。
            element().addEventListener('paste', e => {if (e.clipboardData.files.length) {e.preventDefault();e.stopImmediatePropagation();}}, true);
            element().addEventListener('drop', e => {e.preventDefault();e.stopImmediatePropagation();}, true);
            new MutationObserver(scheduleDecoration).observe(element(), {childList:true,subtree:true,characterData:true});
            scheduleDecoration();
          },0);
        }
      });
    });
  });
})();
