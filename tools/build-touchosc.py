#!/usr/bin/env python3
"""Build a deterministic TouchOSC v6 XML/zlib document; no third-party libraries."""
from pathlib import Path
import uuid
import zlib
import xml.etree.ElementTree as E
ROOT=Path(__file__).resolve().parents[1]
BG=(.045,.06,.075,1); PANEL=(.08,.10,.13,1); DIM=(.14,.18,.21,1)
WHITE=(.93,.95,.96,1); MUTED=(.52,.62,.67,1); A=(.79,.94,.36,1); B=(.30,.79,.94,1)
def prop(node,key,kind,value):
    ps=node.find('properties')
    if ps is None: ps=E.SubElement(node,'properties')
    p=E.SubElement(ps,'property',type=kind); E.SubElement(p,'key').text=key; v=E.SubElement(p,'value')
    if kind in ('c','r'):
        for k,n in zip('rgba' if kind=='c' else 'xywh',value): E.SubElement(v,k).text=str(n)
    else: v.text=str(value)
def node(parent,name,kind,frame,color):
    n=E.SubElement(parent,'node',ID=str(uuid.uuid5(uuid.NAMESPACE_URL,'fm1-beets/'+name)),type=kind)
    for key,t,value in [('name','s',name),('frame','r',frame),('color','c',color),('visible','b',1),('interactive','b',0),('background','b',1),('outline','b',0),('cornerRadius','f',2),('orientation','i',0)]: prop(n,key,t,value)
    return n
def label(name,txt,x,y,w,h,size=16,bg=BG,fg=WHITE):
    n=node(children,name,'LABEL',(x,y,w,h),bg)
    for key,t,v in [('textSize','i',size),('font','i',0),('textColor','c',fg),('textAlignH','i',2),('textAlignV','i',2),('textClip','b',1)]: prop(n,key,t,v)
    vs=E.SubElement(n,'values'); v=E.SubElement(vs,'value'); E.SubElement(v,'key').text='text'
    for k,t in [('locked','0'),('lockedDefaultCurrent','1'),('default',txt),('defaultPull','0')]: E.SubElement(v,k).text=t
    return n
xml=E.Element('lexml',version='6')
root=node(xml,'Beets Visualizer','GROUP',(0,0,1024,768),BG)
prop(root,'script','s',(ROOT/'touchosc/beets.lua').read_text())
children=E.SubElement(root,'children')
label('title','BEETS / FM1',24,18,230,45,32,fg=A)
label('subtitle','TWO DECKS / ONE CLOCK',24,65,230,22,13,fg=MUTED)
label('connection','WAITING FOR FM1',720,24,280,36,17,bg=DIM)
label('transport','--',280,24,136,36,18,bg=DIM)
label('tempo','-- BPM',432,24,130,36,24)
label('mix','A -- / -- B',568,24,145,36,17)
label('focus','FOCUS --',280,68,250,22,13,fg=MUTED)
label('master','MASTER --',720,68,280,22,13,fg=MUTED)
label('clock','STEP -- / 8',530,68,180,22,12,fg=MUTED)
label('phaseback','',24,100,976,4,bg=DIM); label('phase','',24,100,1,4,bg=A)
for d,(prefix,x,accent) in enumerate([('a',24,A),('b',536,B)]):
    label(prefix+'panel','',x-8,124,480,426,bg=PANEL)
    label(prefix+'title','DECK '+prefix.upper(),x,134,108,30,19,bg=accent,fg=WHITE)
    label(prefix+'loop','--',x+118,134,220,30,24,bg=PANEL)
    label(prefix+'flags','--',x,174,464,26,14,bg=DIM)
    label(prefix+'queue','NO LOOP QUEUED',x,207,464,20,12,bg=PANEL,fg=MUTED)
    for i in range(10): label(prefix+'sample'+str(i),str(i+1) if i<8 else 'AM'+str(i-7),x+i*47,234,41,28,12,bg=DIM)
    for i in range(8): label(prefix+'slice'+str(i),str(i+1),x+i*59,277,51,58,24,bg=DIM)
    label(prefix+'positionback','',x,349,464,8,bg=DIM); label(prefix+'position','',x,349,1,8,bg=accent)
    label(prefix+'positiontext','SOURCE -- / WINDOW --',x,363,464,20,12,bg=PANEL,fg=MUTED)
    label(prefix+'voice','--',x,390,464,22,13,bg=PANEL,fg=MUTED)
    for i,k in enumerate(['repeat','skip','reverse','stutter','switch']): label(prefix+k,k.upper()+'\n--%',x+i*94,422,88,65,13,bg=DIM)
    label(prefix+'held','HELD FX:',x,503,464,24,13,bg=PANEL,fg=accent)
label('message','Connect the FM1 Receive Port in MIDI Connections.',24,566,976,30,18,fg=A)
for i,b in enumerate(['FX','SEL','ENV','LFO','EDIT','GLO','HOME','SAVE','ARP','SEQ','PLAY','REC','OCT-','OCT+']): label('btn'+str(i),b,24+i*70,611,64,30,12,bg=DIM)
for i in range(16): label('key'+str(i+1),('A' if i<8 else 'B')+str(i%8+1),24+i*61,650,55,25,12,bg=DIM)
label('input','INPUT / --',24,684,976,24,13,fg=MUTED)
label('diagnostics','RX 0 / WAITING',24,724,976,20,12,fg=MUTED)
data=E.tostring(xml,encoding='utf-8',xml_declaration=True)
(ROOT/'touchosc/Beets.tosc').write_bytes(zlib.compress(data,9))
print('Wrote touchosc/Beets.tosc:',len(children),'controls')
