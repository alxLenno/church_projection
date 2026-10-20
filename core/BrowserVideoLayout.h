#pragma once
#include <QString>
namespace BrowserVideoLayout {
inline QString focus() {
  return QStringLiteral(R"JS((() => {
    const video = document.querySelector('video');
    if (!video) return false;
    if (window.__churchVideoLayout) window.__churchVideoLayout.restore();
    const originals = new Map();
    function set(element, properties) {
      if (!originals.has(element)) originals.set(element, element.getAttribute('style'));
      for (const [key, value] of Object.entries(properties)) element.style.setProperty(key, value, 'important');
    }
    // Direct style properties work on pages whose CSP blocks injected stylesheets.
    // Remove containing blocks/clipping introduced by page layouts and hide
    // siblings, while keeping the original video node and playback session.
    let child = video;
    for (let parent = video.parentElement; parent; child = parent, parent = parent.parentElement) {
      for (const sibling of parent.children) if (sibling !== child) set(sibling, {visibility:'hidden'});
      set(parent, {transform:'none', filter:'none', perspective:'none', contain:'none',
        'will-change':'auto', overflow:'visible', clip:'auto', 'clip-path':'none',
        opacity:'1', visibility:'visible', 'border-radius':'0'});
    }
    for (const root of [document.documentElement, document.body]) set(root, {
      margin:'0', padding:'0', width:'100vw', height:'100vh', overflow:'hidden', background:'black'
    });
    set(video, {position:'fixed', top:'0', left:'0', right:'auto', bottom:'auto',
      width:'100vw', height:'100vh', 'min-width':'0', 'min-height':'0',
      'max-width':'none', 'max-height':'none', margin:'0', padding:'0', border:'0',
      transform:'none', 'object-fit':'contain', 'object-position':'50% 50%',
      'z-index':'2147483647', visibility:'visible', opacity:'1',
      'border-radius':'0', background:'black', 'box-sizing':'border-box'});
    window.__churchVideoLayout = {restore() {
      for (const [element, style] of originals) {
        if (style === null) element.removeAttribute('style'); else element.setAttribute('style', style);
      }
      delete window.__churchVideoLayout;
    }};
    video.muted = false;
    video.play().catch(() => {});
    return true;
  })())JS");
}
inline QString restore() {
  return QStringLiteral("window.__churchVideoLayout?.restore(); document.getElementById('church-projection-video')?.remove(); document.querySelector('video')?.pause();");
}
}
