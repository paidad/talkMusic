/* TalkMusic owns persistence. This page only edits and reports versioned Markdown. */
(() => {
  'use strict';
  let host, editor, generation = 0, revision = 0, original = '', modified = false;
  let muted = false, composing = false, readonly = false, savedRange = null, decorationQueued = false;
  const menu = document.getElementById('menu');
  const element = () => editor.vditor.ir.element;
  const selection = () => window.getSelection();
  const remember = () => { const s = selection(); if (s.rangeCount && element().contains(s.anchorNode)) savedRange = s.getRangeAt(0).cloneRange(); };
  const restore = () => { editor.focus(); if (savedRange && savedRange.startContainer.isConnected) {const s = selection(); s.removeAllRanges(); s.addRange(savedRange);} };
  const report = () => {
    if (muted || composing || readonly) return;
    modified = true; const text = editor.getValue(); ++revision;
    host.changed(generation, revision, text); scheduleDecoration();
  };
  function decorate() {
    decorationQueued = false;
    if (!editor || !editor.vditor.ir || composing) return;
    const tools = document.getElementById('code-tools'); tools.replaceChildren();
    element().querySelectorAll('[data-type="code-block"]').forEach(block => {
      const source = block.querySelector('.vditor-ir__marker--pre > code');
      const rendered = block.querySelector('.vditor-ir__preview');
      // The preview is a display surface. Vditor routes clicks to the source.
      // Never allow browser input to mutate the duplicate rendered representation.
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
  function scheduleDecoration() { if (!decorationQueued) {decorationQueued = true; requestAnimationFrame(decorate);} }
  function toolbar(name) {
    // Invoke the pinned editor's toolbar actions; these participate in its undo stack.
    const button = document.querySelector(`.vditor-toolbar button[data-type="${name}"]`);
    if (button) button.click();
  }
  function command(name) {
    menu.hidden = true; restore();
    if (name === 'undo') editor.vditor.undo.addToUndoStack(editor.vditor);
    if (name === 'copy' || name === 'cut') {
      host.clipboardWrite(editor.getSelection());
      if (name === 'cut') { editor.deleteValue(); report(); }
    } else if (name === 'paste') {
      host.clipboardRead(text => { restore(); editor.insertMD(text); report(); });
    } else if (name === 'image') host.chooseImage();
    else if (/^h[1-6]$/.test(name)) {
      document.querySelector(`.vditor-toolbar button[data-tag="${name}"]`).click(); report();
    } else if (name === 'paragraph') {
      const range = selection().rangeCount ? selection().getRangeAt(0) : null;
      const node = range && (range.startContainer.nodeType === 1 ? range.startContainer : range.startContainer.parentElement);
      const heading = node && node.closest('h1,h2,h3,h4,h5,h6');
      if (heading) {
        // Vditor's current-heading action removes its marker and reparses the block.
        const button = document.querySelector('.vditor-toolbar button[data-type="headings"]');
        button.classList.add('vditor-menu--current'); button.click(); report();
      }
    } else {toolbar(name); report();}
    remember(); scheduleDecoration();
  }
  document.querySelectorAll('#menu button').forEach(button => {
    button.addEventListener('mousedown', e => e.preventDefault());
    button.addEventListener('click', () => command(button.dataset.command));
  });
  document.addEventListener('contextmenu', e => {
    if (!editor || readonly || !element().contains(e.target)) return;
    e.preventDefault(); remember(); menu.hidden = false;
    menu.style.left = `${Math.max(4, Math.min(e.clientX, innerWidth - menu.offsetWidth - 4))}px`;
    menu.style.top = `${Math.max(4, Math.min(e.clientY, innerHeight - menu.offsetHeight - 4))}px`;
  });
  document.addEventListener('mousedown', e => { if (!menu.contains(e.target)) menu.hidden = true; });
  document.addEventListener('keydown', e => {
    if (e.key === 'Escape') menu.hidden = true;
    if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 's') {e.preventDefault(); host.saveRequested();}
  });
  document.addEventListener('selectionchange', () => { if (menu.hidden && editor) remember(); });
  window.addEventListener('resize', scheduleDecoration);
  document.addEventListener('scroll', scheduleDecoration, true);
  window.talkMusic = {
    load(config) {
      muted = true; generation = config.generation; revision = config.revision; original = config.text;
      modified = false; savedRange = null;
      const scroll = element().scrollTop;
      editor.setValue(original, !!config.resetUndo); element().scrollTop = scroll;
      this.readOnly(config.readOnly); muted = false; scheduleDecoration(); host.loaded(generation);
    },
    readOnly(value) {readonly = value; if (value) editor.disabled(); else editor.enable(); element().querySelectorAll('input').forEach(i => {i.disabled=value;}); menu.hidden = true; scheduleDecoration();},
    snapshot() {
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
    // Same action path is used by menus and integration tests.
    command
  };
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
          // Constructor's after callback may run before assignment finishes.
          setTimeout(() => {
            // Keep our typography after the content theme loaded by Vditor.
            document.head.appendChild(document.getElementById('talkMusicTheme'));
            talkMusic.load({...config,resetUndo:true});
            element().addEventListener('compositionstart', () => {composing = true;});
            element().addEventListener('compositionend', () => {composing = false; report();});
            // Flush can run before Vditor's debounced input callback.
            element().addEventListener('input', () => {if (!muted && !readonly) modified = true;});
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
