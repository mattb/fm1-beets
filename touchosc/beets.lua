-- Beets v1 receive-only visualizer. Document root; TouchOSC (not Mk1).
local header = {0xF0,0x7D,0x42,0x45,0x45,0x54,0x53,0x01}
local cache, state = {}, nil
local lastState, lastSeq, messageUntil = nil, nil, 0
local received, gaps, bad = 0, 0, 0
local A, B = Color(.79,.94,.36), Color(.30,.79,.94)
local DIM, OFF, WHITE = Color(.22,.27,.30), Color(.10,.13,.16), Color(.93,.95,.96)
local names = {'repeat','skip','reverse','stutter','switch'}
local buttons = {'FX','SEL','ENV','LFO','EDIT','GLO','HOME','SAVE','ARP','SEQ','PLAY','REC','OCT-','OCT+'}
local encoders = {'TEMPO','SWITCH','BLEND','REPEAT','SKIP','REVERSE','STUTTER'}
local whites = {0,2,4,6,7,9,11,12,14,16,18,19,21,23,24,26}
local function control(n)
  if not cache[n] then cache[n] = self:findByName(n, true) end
  return cache[n]
end
local function text(n, v) local c=control(n); if c then c.values.text=tostring(v) end end
local function paint(n, color, ink)
  local c=control(n); if c then c.color=color; c.textColor=WHITE end
end
local function bit(n,i) return math.floor((tonumber(n) or 0)/2^i)%2==1 end
local function num(n, fallback) return type(n)=='number' and n or (fallback or 0) end
local function clamp(n) return math.max(0, math.min(1,num(n))) end
local function bar(n, v, width)
  local c=control(n); if c then local f=c.frame; f.w=math.max(1,width*clamp(v)); c.frame=f end
end
local function held(keys, btn)
  for i=0,13 do paint('btn'..i,bit(btn,i) and A or OFF,bit(btn,i) and OFF or WHITE) end
  for i=1,16 do paint('key'..i,bit(keys,whites[i]) and (i<9 and A or B) or OFF) end
end
local function render(s)
  local ui=s.ui
  text('transport',s.playing==1 and 'PLAYING' or 'STOPPED')
  paint('transport',s.playing==1 and A or DIM,s.playing==1 and OFF or WHITE)
  text('tempo',s.tempo..' BPM')
  text('mix','A '..(100-s.blend)..' / '..s.blend..' B')
  text('master','MASTER '..math.floor(num(ui.master)/4096*100)..'%')
  text('focus','FOCUS '..(ui.focus==1 and 'B' or 'A')..(ui.dirty==1 and '  /  UNSAVED' or ''))
  text('clock','STEP '..(num(s.step)+1)..' / 8   |   TICK '..num(s.ticks))
  bar('phase',num(s.clock_phase)/2646000,976)
  held(ui.keys,ui.buttons)
  for d=1,2 do
    local l,p=s.lanes[d],s.playheads[d]
    local prefix=d==1 and 'a' or 'b'
    local accent=d==1 and A or B
    text(prefix..'loop',l.loop_name or '--')
    text(prefix..'queue',num(l.queued_loop,-1)>=0 and ('NEXT: '..(l.queued_loop_name or '?')) or 'NO LOOP QUEUED')
    local flags=(l.muted==1 and 'MUTED' or 'UNMUTED')..(l.bypass==1 and ' / BYPASS' or '')..(l.auto_next==1 and ' / AUTO' or '')
    text(prefix..'flags',flags)
    paint(prefix..'flags',l.muted==1 and DIM or accent,l.muted==1 and WHITE or OFF)
    text(prefix..'voice',(p.direction==-1 and 'REVERSE' or 'FORWARD')..'  /  '..(p.active==1 and 'ACTIVE' or 'IDLE')..'  /  GAIN '..math.floor(num(p.gain_ppm)/10000)..'%')
    bar(prefix..'position',num(p.position_ppm)/1000000,464)
    text(prefix..'positiontext','SOURCE '..string.format('%.1f',num(p.position_ppm)/10000)..'%  |  WINDOW '..math.floor(num(p.window_start_ppm)/10000)..'-'..math.floor(num(p.window_end_ppm)/10000)..'%')
    for i=0,7 do
      local selected=i==l.slice
      local eligible=bit(l.slice_mask,i) and i>=num(l.first) and i<=num(l.last)
      paint(prefix..'slice'..i,selected and accent or (eligible and DIM or OFF),selected and OFF or WHITE)
      text(prefix..'slice'..i,tostring(i+1)..(i==l.queued_slice and ' >' or ''))
    end
    for i=0,9 do
      local selected=i==l.loop
      paint(prefix..'sample'..i,selected and accent or (bit(l.loop_mask,i) and DIM or OFF),selected and OFF or WHITE)
      text(prefix..'sample'..i,(i<8 and tostring(i+1) or ('AM'..(i-7)))..(i==l.queued_loop and ' >' or ''))
    end
    for i,k in ipairs(names) do
      text(prefix..k,string.upper(k)..'\n'..num(l.probability[k])..'%')
      paint(prefix..k,bit(l.events,i-1) and accent or OFF,bit(l.events,i-1) and OFF or WHITE)
    end
    text(prefix..'held','HELD FX: '..(l.force_reverse==1 and 'REVERSE ' or '')..(l.force_stutter==1 and 'STUTTER' or ''))
  end
  text('message',ui.message~='' and ui.message or 'Play the FM1. This panel follows its state.')
  messageUntil=getMillis()+num(ui.message_remaining_ms)
end
local function valid(s)
  if type(s.ui)~='table' or type(s.lanes)~='table' or type(s.playheads)~='table' then return false end
  if type(s.tempo)~='number' or type(s.blend)~='number' then return false end
  for d=1,2 do
    if type(s.lanes[d])~='table' or type(s.lanes[d].probability)~='table' or type(s.playheads[d])~='table' then return false end
  end
  return true
end
function onReceiveMIDI(message, connections)
  if #message<#header+2 then return end
  for i=1,#header do if message[i]~=header[i] then return end end
  if message[#message]~=0xF7 then return end
  if #message>3072 then bad=bad+1; return true end
  local chars={}
  for i=#header+1,#message-1 do
    if message[i]<32 or message[i]>126 then bad=bad+1; return true end
    chars[#chars+1]=string.char(message[i])
  end
  -- TouchOSC omits pcall from its Lua sandbox. Native JSON returns nil on failure.
  local data=json.toTable(table.concat(chars))
  if type(data)~='table' or data.v~=1 or type(data.seq)~='number' then bad=bad+1; return true end
  if data.type~='state' and data.type~='input' then return true end
  if data.type=='state' and not valid(data) then bad=bad+1; return true end
  if lastState and getMillis()-lastState>3000 then lastSeq=nil end
  if lastSeq and (data.seq-lastSeq)%4294967296~=1 then gaps=gaps+1 end
  lastSeq=data.seq; received=received+1
  if data.type=='state' then
    state=data; lastState=getMillis(); render(data)
  else
    local parts={}
    if num(data.keys_down)~=0 then parts[#parts+1]='KEY DOWN' end
    if num(data.keys_up)~=0 then parts[#parts+1]='KEY UP' end
    for i=0,13 do
      if bit(data.buttons_down,i) then parts[#parts+1]=buttons[i+1]..' DOWN' end
      if bit(data.buttons_up,i) then parts[#parts+1]=buttons[i+1]..' UP' end
    end
    if type(data.enc)=='table' then
      for i=1,7 do if num(data.enc[i])~=0 then parts[#parts+1]=encoders[i]..' '..string.format('%+d',data.enc[i]) end end
    end
    text('input',table.concat(parts,'  /  '))
  end
  return true
end
function update()
  local age=lastState and getMillis()-lastState or nil
  local live=age and age<3000
  text('connection',live and 'USB MIDI / LIVE' or (age and 'STALE / CHECK USB' or 'WAITING FOR FM1'))
  paint('connection',live and B or DIM,live and OFF or WHITE)
  text('diagnostics','RX '..received..'   SEQ '..(lastSeq or '--')..'   GAPS '..gaps..'   BAD '..bad..'   DROPPED '..(state and num(state.ui.input_dropped) or 0))
  if not live then held(0,0) end
  if state and getMillis()>messageUntil then text('message','Play the FM1. This panel follows its state.') end
end
