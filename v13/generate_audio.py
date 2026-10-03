#!/usr/bin/env python3
from pathlib import Path
import math, sys, wave
import numpy as np

SR=44100
OUT=Path(sys.argv[1] if len(sys.argv)>1 else 'assets/audio')
OUT.mkdir(parents=True,exist_ok=True)

def save(path,a):
    a=np.asarray(a,dtype=np.float64)
    if a.ndim==1: a=np.column_stack([a,a])
    p=np.max(np.abs(a)) if a.size else 1.0
    if p>0.96: a*=0.96/p
    pcm=(a*32767).astype(np.int16)
    with wave.open(str(path),'wb') as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR); w.writeframes(pcm.tobytes())

def envelope(n,a=.005,r=.05):
    e=np.ones(n); na=min(n,int(a*SR)); nr=min(n-na,int(r*SR))
    if na: e[:na]=np.linspace(0,1,na,endpoint=False)
    if nr: e[-nr:]=np.linspace(1,0,nr)
    return e

def add(buf,start,dur,freq,amp=.2,kind='sine',pan=0.0):
    i0=max(0,int(round(start*SR))); i1=min(len(buf),int(round((start+dur)*SR))); n=i1-i0
    if n<=0:return
    t=np.arange(n)/SR
    if kind=='softsaw':
        s=sum((1/k)*np.sin(2*np.pi*freq*k*t) for k in range(1,6)); s/=np.max(np.abs(s))+1e-9
    elif kind=='choir':
        s=.60*np.sin(2*np.pi*freq*t)+.22*np.sin(2*np.pi*freq*2*t+.4)+.12*np.sin(2*np.pi*freq*3*t+1.1)+.06*np.sin(2*np.pi*freq*4*t+.7)
        s*=.86+.14*np.sin(2*np.pi*.27*t)
    else:s=np.sin(2*np.pi*freq*t)
    s*=envelope(n,min(.6,dur*.25),min(.8,dur*.3))*amp
    l=math.cos((pan+1)*math.pi/4); r=math.sin((pan+1)*math.pi/4)
    buf[i0:i1,0]+=s*l; buf[i0:i1,1]+=s*r

def lowpass(x,cutoff):
    alpha=1-np.exp(-2*np.pi*cutoff/SR); y=np.empty_like(x); y[0]=x[0]
    for i in range(1,len(x)): y[i]=y[i-1]+alpha*(x[i]-y[i-1])
    return y

# Background music: original dark cinematic 48 s loop, slightly louder.
duration=48.0; n=int(SR*duration); t=np.arange(n)/SR; bg=np.zeros((n,2)); rng=np.random.default_rng(7)
bpm=72; beat=60/bpm; bar=4*beat
chords=[[73.42,110.00,146.83],[58.27,87.31,116.54],[65.41,98.00,130.81],[73.42,110.00,174.61]]
for bi in range(int(np.ceil(duration/bar))):
    st=bi*bar; dur=min(bar,duration-st); c=chords[bi%4]
    add(bg,st,dur,c[0],.105,'softsaw',-.18); add(bg,st,dur,c[1],.068,'sine',.10); add(bg,st,dur,c[2],.055,'choir',.18)
sub=.030*np.sin(2*np.pi*36.71*t)*(.84+.16*np.sin(2*np.pi*.05*t)); bg[:,0]+=sub; bg[:,1]+=sub

def drum(start,amp=.45,freq=50,decay=.75,pan=0):
    i0=max(0,int(round(start*SR))); N=min(int(1.35*SR),len(bg)-i0)
    if N<=0:return
    tt=np.arange(N)/SR; phase=2*np.pi*(freq*tt+3.6*(1-np.exp(-tt*7)))
    body=np.sin(phase)*np.exp(-tt/decay); noise=lowpass(rng.normal(0,1,N),1000)*np.exp(-tt/.10)
    hit=amp*(.90*body+.10*noise); l=math.cos((pan+1)*math.pi/4); r=math.sin((pan+1)*math.pi/4)
    bg[i0:i0+N,0]+=hit*l; bg[i0:i0+N,1]+=hit*r
for b in np.arange(0,duration,bar):
    drum(b,.46,49,.80,-.05)
    if b+2*beat<duration: drum(b+2*beat,.31,55,.55,.08)
for b in np.arange(bar,duration,2*bar):
    if b+3*beat<duration: drum(b+3*beat,.19,72,.38,.25)
motif=[(0,293.66),(1.5*beat,261.63),(3*beat,220.00),(4*beat,233.08),(6*beat,261.63),(7*beat,293.66)]
for base in [8,24,40]:
    for off,f in motif:
        if base+off<duration: add(bg,base+off,min(1,duration-base-off),f,.060,'choir',-.1 if int(off/beat)%2==0 else .15)
dry=bg.copy()
for ds,g in [(.18,.16),(.36,.10),(.72,.06)]:
    d=int(ds*SR); bg[d:]+=dry[:-d]*g
for c in range(2): bg[:,c]=lowpass(bg[:,c],6500)
fade=int(1.0*SR); bg[:fade]*=np.linspace(0,1,fade)[:,None]; bg[-fade:]*=np.linspace(1,.85,fade)[:,None]
bg=np.tanh(bg*1.30); bg*=.94/(np.max(np.abs(bg))+1e-9)
save(OUT/'background.wav',bg)

# Happy 8-second startup jingle.
duration=8.0; n=int(SR*duration); intro=np.zeros((n,2))
notes={'C4':261.63,'E4':329.63,'G4':392.00,'A4':440.00,'C5':523.25,'E5':659.25,'G5':783.99,'A5':880.00,'C6':1046.50}
mel=[(0,.42,'C5'),(.45,.42,'E5'),(.90,.42,'G5'),(1.35,.68,'C6'),(2.25,.4,'A5'),(2.68,.4,'G5'),(3.11,.4,'E5'),(3.54,.72,'G5'),(4.55,.34,'E5'),(4.92,.34,'G5'),(5.29,.34,'A5'),(5.66,.42,'G5'),(6.18,1.35,'C6')]
for i,(st,d,key) in enumerate(mel): add(intro,st,d,notes[key],.23,'choir',-.18 if i%2==0 else .18)
for st,ch in [(0,['C4','E4','G4']),(2.1,['A4','C5','E5']),(4.35,['G4','C5','E5']),(6.05,['C4','E4','G4'])]:
    for j,key in enumerate(ch): add(intro,st,1.75,notes[key],.075,'sine',-.25+.25*j)
dry=intro.copy()
for ds,g in [(.11,.20),(.24,.11),(.43,.06)]:
    d=int(ds*SR); intro[d:]+=dry[:-d]*g
intro[-int(.55*SR):]*=np.linspace(1,0,int(.55*SR))[:,None]
intro*=.91/(np.max(np.abs(intro))+1e-9)
save(OUT/'startup.wav',intro)

# UI sound effects.
def stereo_tone(dur,func,amp=.9,pan=0):
    n=int(SR*dur); t=np.arange(n)/SR; x=func(t)*envelope(n,.002,min(.08,dur*.5))*amp
    l=math.cos((pan+1)*math.pi/4); r=math.sin((pan+1)*math.pi/4); return np.column_stack([x*l,x*r])

save(OUT/'move.wav',stereo_tone(.085,lambda t:(.55*np.sin(2*np.pi*950*t)+.25*np.sin(2*np.pi*1450*t))*np.exp(-t/.028),.9,-.05))
save(OUT/'confirm.wav',stereo_tone(.16,lambda t:(.55*np.sin(2*np.pi*(620*t+180*t*t/.16))+.28*np.sin(4*np.pi*(620*t+180*t*t/.16)))*np.exp(-t/.07),.9,.08))
save(OUT/'back.wav',stereo_tone(.20,lambda t:(.52*np.sin(2*np.pi*(520*t-260*t*t/(2*.20)))+.14*np.sin(4*np.pi*(520*t-260*t*t/(2*.20))))*np.exp(-t/.11),.85,-.08))

n=int(SR*1.15); unlock=np.zeros((n,2))
for i,(st,f) in enumerate([(0,659.25),(.18,783.99),(.36,987.77),(.54,1318.51)]):
    i0=int(st*SR); N=min(int(.6*SR),n-i0); tt=np.arange(N)/SR
    s=(np.sin(2*np.pi*f*tt)+.35*np.sin(2*np.pi*2.01*f*tt)+.16*np.sin(2*np.pi*3.98*f*tt))*np.exp(-tt/.28)*.28
    pan=-.25+.17*i; l=math.cos((pan+1)*math.pi/4); r=math.sin((pan+1)*math.pi/4)
    unlock[i0:i0+N,0]+=s*l; unlock[i0:i0+N,1]+=s*r
save(OUT/'unlock.wav',unlock)

for p in OUT.glob('*.wav'): print('[OK] audio',p.name,p.stat().st_size)
