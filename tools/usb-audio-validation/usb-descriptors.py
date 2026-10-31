#!/usr/bin/env python3
"""Read actual USB configuration/channel strings and UAC1 capture GET_CUR rate.
No configuration/alternate-setting writes, kernel-driver detach, reset or interface
claim. macOS may refuse concurrent access; that is reported rather than bypassed.
Requires pyusb and libusb. Product selection is explicit; no default device.
"""
import argparse,json
import usb.core, usb.util
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--product',required=True);p.add_argument('--vid',type=lambda x:int(x,0),default=0x1209);p.add_argument('--get-rate',action='store_true');a=p.parse_args()
found=[]
for d in usb.core.find(find_all=True,idVendor=a.vid):
 try:
  if a.product.casefold() in (d.product or '').casefold():found.append(d)
 except usb.core.USBError: pass
if len(found)!=1:raise SystemExit(f'Expected one accessible matching USB device; found {len(found)}. No driver detached.')
d=found[0]
try:
 header=bytes(d.ctrl_transfer(0x80,6,0x0200,0,9));total=int.from_bytes(header[2:4],'little')
 data=bytes(d.ctrl_transfer(0x80,6,0x0200,0,total));assert len(data)==total
 records=[];offset=0;interface=None;capture=[];terminals=[]
 while offset<len(data):
  n=data[offset];assert n>=2 and offset+n<=len(data)
  b=data[offset:offset+n];typ=b[1]
  if typ==4:interface={'number':b[2],'alt':b[3],'class':b[5],'subclass':b[6]}
  item={'offset':offset,'bytes':b.hex(),'interface':interface}
  if typ==0x24 and interface and interface['class']==1:
   if interface['subclass']==1 and len(b)==12 and b[2]==2:
    first=b[10];names=[usb.util.get_string(d,first+i) for i in range(b[7])] if first else []
    terminal={'id':b[3],'channels':b[7],'channel_config':int.from_bytes(b[8:10],'little'),'first_name_string':first,'names':names};terminals.append(terminal);item['input_terminal']=terminal
   if interface['subclass']==2 and b[2]==2 and b[3]==1:
    count=b[7];rates=[int.from_bytes(b[8+3*i:11+3*i],'little') for i in range(count)]
    item['pcm_format']={'channels':b[4],'subframe_bytes':b[5],'bits':b[6],'rates':rates}
   if interface['subclass']==2 and b[2]==1:item['terminal_link']=b[3]
  if typ==5 and interface and interface['class']==1 and interface['subclass']==2:
   ep={'direction':'capture' if b[2]&0x80 else 'playback','address':b[2],'max_packet':int.from_bytes(b[4:6],'little'),'interval':b[6]}
   capture.append(ep)
  if typ==0x25 and interface and interface['subclass']==2 and capture:
   capture[-1]['sampling_frequency_control']=bool(b[3]&1)
  records.append(item);offset+=n
 if a.get_rate:
  for ep in capture:
   if ep.get('sampling_frequency_control'):
    try:ep['current_rate']=int.from_bytes(bytes(d.ctrl_transfer(0xa2,0x81,0x0100,ep['address'],3)),'little')
    except usb.core.USBError as e:ep['get_cur_error']=str(e)
 print(json.dumps({'product':d.product,'vid':d.idVendor,'pid':d.idProduct,'bcdDevice':d.bcdDevice,'input_terminals':terminals,'capture_endpoints':[e for e in capture if e['direction']=='capture'],'playback_endpoints':[e for e in capture if e['direction']=='playback'],'configuration':records},indent=2))
finally:usb.util.dispose_resources(d)
