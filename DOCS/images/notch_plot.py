"""Plots for PFC_Software_Architecture_CN.md / _EN.md section 8.2a (voltage-loop notch).
Run: python notch_plot.py   (needs numpy, matplotlib; Microsoft YaHei font on Windows)
Writes notch_*.png next to this script.
"""
import numpy as np, math, matplotlib
from pathlib import Path
OUT=Path(__file__).resolve().parent
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import font_manager as fm
fm.fontManager.addfont(r"C:\Windows\Fonts\msyh.ttc")
plt.rcParams["font.family"]="Microsoft YaHei"; plt.rcParams["axes.unicode_minus"]=False

Ts=8/65000; fn=100.0; BW=20.0
w0=2*np.pi*fn; sig=np.pi*BW
f=np.logspace(1,3.2,6000)          # 10 Hz .. 1.6 kHz
s=1j*2*np.pi*f

def db(h): return 20*np.log10(np.maximum(np.abs(h),1e-6))
def ph(h): return np.degrees(np.unwrap(np.angle(h)))

# s domain pieces
Hz_num = (s**2+w0**2)/w0**2                    # numerator only (normalised DC=1)
Hp     = w0**2/(s**2+2*sig*s+w0**2)            # denominator only (DC=1)
Hs     = (s**2+w0**2)/(s**2+2*sig*s+w0**2)

# z domain (code)
r=1-np.pi*BW*Ts; c=np.cos(w0*Ts); K=(1-2*r*c+r*r)/(2-2*c)
z=np.exp(1j*2*np.pi*f*Ts)
Hz=K*(1-2*c/z+1/z**2)/(1-2*r*c/z+r*r/z**2)

# ---- Fig1: build-up in s domain
fig,ax=plt.subplots(2,1,figsize=(9,7),sharex=True)
ax[0].semilogx(f,db(Hz_num),"--",c="tab:orange",label="只有零点  (s²+ω0²)/ω0²")
ax[0].semilogx(f,db(Hp),"--",c="tab:green",label="只有极点  ω0²/(s²+2σs+ω0²)")
ax[0].semilogx(f,db(Hs),c="tab:blue",lw=2.2,label="两者相乘 = 陷波器 H(s)")
ax[0].set_ylim(-60,30); ax[0].axvline(fn,c="gray",ls=":"); ax[0].set_ylabel("幅值 [dB]")
ax[0].grid(True,which="both",alpha=.3); ax[0].legend(loc="lower left",fontsize=9)
ax[0].set_title("s 域：零点 × 极点 = 陷波器（fn = 100 Hz，BW = 20 Hz）")
ax[1].semilogx(f,ph(Hs),c="tab:blue",lw=2.2)
ax[1].set_ylabel("相位 [°]"); ax[1].set_xlabel("频率 [Hz]"); ax[1].axvline(fn,c="gray",ls=":")
ax[1].grid(True,which="both",alpha=.3)
plt.tight_layout(); plt.savefig(OUT/"notch_s_build.png",dpi=110); plt.close()

# ---- Fig2: width controlled by sigma (BW)
fig,ax=plt.subplots(2,1,figsize=(9,7),sharex=True)
for bw,cl in [(5,"tab:red"),(20,"tab:blue"),(60,"tab:green")]:
    sg=np.pi*bw; h=(s**2+w0**2)/(s**2+2*sg*s+w0**2)
    ax[0].semilogx(f,db(h),c=cl,lw=2,label=f"BW = {bw} Hz  (σ = {sg:.0f})")
    ax[1].semilogx(f,ph(h),c=cl,lw=2)
ax[0].set_ylim(-40,5); ax[0].axvline(fn,c="gray",ls=":"); ax[0].set_ylabel("幅值 [dB]")
ax[0].legend(); ax[0].grid(True,which="both",alpha=.3)
ax[0].set_title("极点离虚轴越近（σ 越小），凹陷越窄、相位滞后越小")
ax[1].set_ylabel("相位 [°]"); ax[1].set_xlabel("频率 [Hz]"); ax[1].axvline(fn,c="gray",ls=":"); ax[1].grid(True,which="both",alpha=.3)
plt.tight_layout(); plt.savefig(OUT/"notch_s_width.png",dpi=110); plt.close()

# ---- Fig3: s vs z
fig,ax=plt.subplots(2,1,figsize=(9,7),sharex=True)
ax[0].semilogx(f,db(Hs),c="tab:blue",lw=2.5,label="s 域（连续）")
ax[0].semilogx(f,db(Hz),"--",c="tab:red",lw=1.8,label="z 域（代码系数）")
ax[0].set_ylim(-40,5); ax[0].axvline(fn,c="gray",ls=":"); ax[0].set_ylabel("幅值 [dB]")
ax[0].legend(); ax[0].grid(True,which="both",alpha=.3)
ax[0].set_title("s 域 vs z 域（Ts = 123 µs，奈奎斯特 4.06 kHz）")
ax[1].semilogx(f,ph(Hs),c="tab:blue",lw=2.5); ax[1].semilogx(f,ph(Hz),"--",c="tab:red",lw=1.8)
ax[1].set_ylabel("相位 [°]"); ax[1].set_xlabel("频率 [Hz]"); ax[1].axvline(fn,c="gray",ls=":"); ax[1].grid(True,which="both",alpha=.3)
plt.tight_layout(); plt.savefig(OUT/"notch_s_vs_z.png",dpi=110); plt.close()

# ---- Fig4: pole-zero maps
fig,ax=plt.subplots(1,2,figsize=(11,4.8))
a=ax[0]; a.axhline(0,c="k",lw=.6); a.axvline(0,c="k",lw=.6)
a.plot([0,0],[w0,-w0],"o",mfc="none",ms=11,c="tab:orange",label="零点  ±jω0")
a.plot([-sig,-sig],[w0,-w0],"x",ms=11,mew=2.5,c="tab:blue",label="极点  −σ ± jω0")
a.annotate("",xy=(-sig,w0*1.07),xytext=(0,w0*1.07),arrowprops=dict(arrowstyle="<->"))
a.text(-sig/2,w0*1.12,"σ",ha="center")
a.set_xlim(-300,150); a.set_ylim(-800,800); a.set_xlabel("σ (实部)"); a.set_ylabel("jω (虚部) [rad/s]")
a.set_title("s 平面"); a.legend(loc="center left"); a.grid(alpha=.3)
a=ax[1]; t=np.linspace(0,2*np.pi,400); a.plot(np.cos(t),np.sin(t),c="gray",lw=1)
a.axhline(0,c="k",lw=.6); a.axvline(0,c="k",lw=.6)
a.plot([np.cos(w0*Ts)]*2,[np.sin(w0*Ts),-np.sin(w0*Ts)],"o",mfc="none",ms=11,c="tab:orange",label="零点  e^(±jw0)  (|z|=1)")
a.plot([r*np.cos(w0*Ts)]*2,[r*np.sin(w0*Ts),-r*np.sin(w0*Ts)],"x",ms=11,mew=2.5,c="tab:blue",label=f"极点  r·e^(±jw0)  (r={r:.4f})")
a.set_xlim(0.95,1.01); a.set_ylim(-0.12,0.12); a.set_aspect("auto")
a.set_xlabel("Re"); a.set_ylabel("Im"); a.set_title("z 平面（放大单位圆右侧）"); a.legend(loc="lower left",fontsize=8); a.grid(alpha=.3)
plt.tight_layout(); plt.savefig(OUT/"notch_pole_zero.png",dpi=110); plt.close()

# numbers for text
def at(h,ff):
    i=np.argmin(abs(f-ff)); return db(h)[i],ph(h)[i]
for ff in [20,50,90,100,110,150]:
    a=at(Hs,ff);b=at(Hz,ff); print(f"{ff:4d}Hz  s: {a[0]:7.2f}dB {a[1]:7.2f}deg | z: {b[0]:7.2f}dB {b[1]:7.2f}deg")
print("sigma",sig,"w0",w0,"r",r)
