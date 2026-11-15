import math, numpy as np
from PIL import Image
from scipy.interpolate import PchipInterpolator
BAYER=np.array([[0,8,2,10],[12,4,14,6],[3,11,1,9],[15,7,13,5]])/16.0
def P(pts):
    pts=np.array(pts,float); f=PchipInterpolator(pts[:,0],pts[:,1]); return f,f.derivative(),pts[0,0],pts[-1,0]
class Cloud:
    def __init__(s): s.p=[];s.n=[];s.m=[]
    def add(s,p,n,m): s.p.append(p.reshape(-1,3)); s.n.append(n.reshape(-1,3)); s.m.append(np.full(len(p.reshape(-1,3)),m))
    def rev(s,prof,a0,a1,mat,inward=False,cut=None,res=330,ph0=0,ph1=2*math.pi):
        f,df,_,_=prof; n_a=int((a1-a0)*res*1.4)+2; rmax=float(np.max(f(np.linspace(a0,a1,50))))
        n_p=int(rmax*(ph1-ph0)*res*1.4)+4
        a=np.linspace(a0,a1,n_a)[:,None]; ph=np.linspace(ph0,ph1,n_p,endpoint=False)[None,:]
        r=f(a); dr=df(a); nn=np.sqrt(1+dr*dr)
        X=np.broadcast_to(a,(n_a,n_p)); R=np.broadcast_to(r,(n_a,n_p)); PH=np.broadcast_to(ph,(n_a,n_p))
        pos=np.stack([X,R*np.cos(PH),R*np.sin(PH)],-1)
        sg=-1 if inward else 1
        nrm=np.stack([np.broadcast_to(-dr/nn*sg,(n_a,n_p)),np.broadcast_to(1/nn*sg,(n_a,n_p))*np.cos(PH),np.broadcast_to(1/nn*sg,(n_a,n_p))*np.sin(PH)],-1)
        pos=pos.reshape(-1,3); nrm=nrm.reshape(-1,3)
        if cut is not None:
            keep=cut(pos); pos=pos[keep]; nrm=nrm[keep]
        s.add(pos,nrm,mat)
def render(out_wh, ppu, yaw=24, pitch=-12, roll=11, cutthr=0.3, D=9.0, N=18):
    SS=3; Wf,Hf=out_wh; W,H=Wf*SS,Hf*SS; res=ppu*SS
    ps,th,ro=map(math.radians,(yaw,pitch,roll))
    Rz=np.array([[math.cos(ps),-math.sin(ps),0],[math.sin(ps),math.cos(ps),0],[0,0,1]])
    Rx=np.array([[1,0,0],[0,math.cos(th),-math.sin(th)],[0,math.sin(th),math.cos(th)]])
    Ry=np.array([[math.cos(ro),0,math.sin(ro)],[0,1,0],[-math.sin(ro),0,math.cos(ro)]])
    Rm=Ry@Rx@Rz
    camdir=Rm.T@np.array([0,-1,0.0])      # world direction towards the camera
    def cutf(a0):
        def f(p):
            rad=np.stack([np.zeros(len(p)),p[:,1],p[:,2]],1); rn=np.linalg.norm(rad,axis=1)+1e-9
            return ~(((rad@camdir)/rn>cutthr)&(p[:,0]>a0))
        return f
    C=Cloud()
    # --- spinner + hub
    C.rev(P([(0,0),(0.04,0.06),(0.2,0.2),(0.4,0.3),(0.55,0.33)]),0,0.55,6,res=res)
    # --- shaft
    C.rev(P([(0.5,0.075),(3.4,0.075)]),0.5,3.4,5,res=res)
    # --- nacelle (outer + inner), cut away on the camera side behind the lip
    outer=P([(0.0,1.0),(0.025,1.08),(0.12,1.16),(0.5,1.21),(1.1,1.15),(1.7,0.98),(2.2,0.80),(2.55,0.70)])
    inner=P([(0.0,1.0),(0.12,1.04),(0.5,1.10),(1.1,1.08),(1.7,0.90),(2.2,0.74),(2.55,0.64)])
    C.rev(outer,0,2.55,2,cut=cutf(0.34),res=res); C.rev(inner,0,2.55,7,inward=True,cut=cutf(0.34),res=res)
    # nacelle rear lip ring
    C.rev(P([(2.55,0.64),(2.55,0.70)]),2.55,2.5501,2,res=res) if False else None
    # --- core cowl
    core=P([(0.55,0.33),(0.8,0.34),(1.2,0.36),(1.9,0.40),(2.4,0.44),(2.9,0.42),(3.25,0.33),(3.35,0.28)])
    C.rev(core,0.55,3.35,3,cut=cutf(0.7),res=res)
    C.rev(P([(0.7,0.30),(1.9,0.37),(2.9,0.39),(3.25,0.30)]),0.7,3.25,8,inward=True,cut=cutf(0.7),res=res)
    cone=P([(3.35,0.28),(3.55,0.2),(3.85,0.07),(3.98,0.0)]); C.rev(cone,3.35,3.98,3,res=res)
    # --- fan blades
    for i in range(N):
        ph0=2*math.pi*i/N
        t=np.linspace(0,1,int(0.75*res*1.5)); s=np.linspace(-0.5,0.5,int(0.30*res*1.4))
        T,Sg=np.meshgrid(t,s,indexing="ij")
        r=0.30+0.70*T
        chord=0.20+0.24*np.sin(math.pi*np.clip(T*0.97,0,1))**0.6+0.02
        stag=np.radians(35+32*T)
        a=0.34-0.20*T**1.4+Sg*chord*np.cos(stag)
        tang=Sg*chord*np.sin(stag)
        ph=ph0+0.30*T**1.8+tang/r
        pos=np.stack([a,r*np.cos(ph),r*np.sin(ph)],-1)
        # normal: cross of d/dt and d/ds
        dt=np.gradient(pos,axis=0); ds=np.gradient(pos,axis=1)
        nrm=np.cross(dt,ds); nrm/=np.linalg.norm(nrm,axis=-1,keepdims=True)+1e-9
        C.add(pos,nrm,1)
    # --- OGV vanes in the bypass
    for k in range(30):
        phk=2*math.pi*k/30
        r=np.linspace(0.36,1.06,int(0.7*res)); a=np.linspace(1.1,1.22,int(0.12*res*1.5))
        R,A=np.meshgrid(r,a,indexing="ij")
        pos=np.stack([A+0.05*(R-0.36),R*np.cos(phk),R*np.sin(phk)],-1).reshape(-1,3)
        nrm=np.tile(np.array([0.2,-math.sin(phk),math.cos(phk)]),(len(pos),1))
        keep=cutf(0.34)(pos); C.add(pos[keep],nrm[keep],4)
    # --- compressor / turbine stage rotors (visible through the cut)
    stages=[(0.75+0.065*j,0.12,0.30-0.0) for j in range(14)]+[(2.35+0.1*j,0.12,0.40) for j in range(5)]
    for (a0,r0,r1) in stages:
        rr=min(r1,float(core[0](a0))-0.03)
        for k in range(40):
            phk=2*math.pi*k/40
            r=np.linspace(0.10,rr,int(0.25*res)); a=np.linspace(a0,a0+0.035,int(0.04*res*1.5)+2)
            R,A=np.meshgrid(r,a,indexing="ij")
            pos=np.stack([A+0.03*(R-0.1),R*np.cos(phk),R*np.sin(phk)],-1).reshape(-1,3)
            nrm=np.tile(np.array([0.3,-math.sin(phk),math.cos(phk)]),(len(pos),1))
            keep=cutf(0.7)(pos); C.add(pos[keep],nrm[keep],9)
    pos=np.concatenate(C.p); nrm=np.concatenate(C.n); mat=np.concatenate(C.m)
    # --- camera
    q=pos@Rm.T; nq=nrm@Rm.T
    dep=q[:,1]; sc=D/(D+dep)
    X=q[:,0]*sc; Z=q[:,2]*sc
    xmin,xmax,zmin,zmax=X.min(),X.max(),Z.min(),Z.max()
    ox=W*0.5-(xmin+xmax)/2*res; oy=H*0.5+(zmin+zmax)/2*res
    px=np.round(X*res+ox).astype(int); py=np.round(-Z*res+oy).astype(int)
    ok=(px>=0)&(px<W)&(py>=0)&(py<H); px,py,dep,nq,mat=px[ok],py[ok],dep[ok],nq[ok],mat[ok]
    idx=py*W+px; order=np.argsort(-dep,kind="stable")
    zb=np.full(W*H,np.nan); mb=np.zeros(W*H,np.int8); sh=np.zeros(W*H)
    L=np.array([-0.45,-0.6,0.65]); L/=np.linalg.norm(L)
    nn=nq/ (np.linalg.norm(nq,axis=1,keepdims=True)+1e-9)
    back=nn[:,1]>0; nn[back]*=-1
    lam=np.clip(nn@L,0,1)
    base={1:0.55,2:0.9,3:0.8,4:0.75,5:0.35,6:0.7,7:0.55,8:0.4,9:0.5}
    tone=np.array([base[int(m)] for m in range(10) if m in base] and [0]) if False else None
    tb=np.array([0,0.75,1.0,0.95,0.92,0.55,0.85,0.8,0.7,0.75])
    val=(0.38+0.62*lam)*tb[mat]; val=np.where(back,val*0.72,val)
    o=order; zb[idx[o]]=dep[o]; mb[idx[o]]=mat[o]; sh[idx[o]]=val[o]
    zb=zb.reshape(H,W); mb=mb.reshape(H,W); sh=sh.reshape(H,W); has=mb>0
    # fill 1px holes
    from scipy import ndimage as ndi
    cnt=ndi.convolve(has.astype(float),np.ones((3,3)),mode="constant")
    hole=(~has)&(cnt>=6)
    for arr in (sh,zb):
        pass
    mx=ndi.maximum_filter(np.where(has,sh,0),3); sh=np.where(hole,mx,sh)
    mi=ndi.maximum_filter(np.where(has,mb,0),3); mb=np.where(hole,mi,mb); has=mb>0
    zf=np.where(np.isnan(zb),0,zb); zf=np.where(hole,ndi.maximum_filter(zf,3),zf)
    # edges: material change or depth jump
    e=np.zeros((H,W),bool)
    for dx,dy in ((1,0),(0,1)):
        a=mb[:H-dy,:W-dx]; b=mb[dy:,dx:]
        da=np.abs(zf[:H-dy,:W-dx]-zf[dy:,dx:])
        m=(a!=b)|(da>0.05)
        e[:H-dy,:W-dx]|=m
    e=ndi.binary_dilation(e,iterations=1)
    # downsample
    def ds(a): return a.reshape(Hf,SS,Wf,SS).mean((1,3))
    shf=ds(np.where(has,sh,1.0)); ef=ds(e.astype(float)); hf=ds(has.astype(float))
    mf=mb[SS//2::SS,SS//2::SS]
    yy,xx=np.mgrid[0:Hf,0:Wf]
    thr=BAYER[yy%4,xx%4]
    black=np.isin(mf,(1,5,6,9,4,8,3))                 # solid-ink materials
    light=np.isin(mf,(2,7))                           # shells: paper with light shading
    shade=(shf<thr*0.55)                              # sparse dots in the shadows of shells
    base=np.where(black,True,np.where(light,shade&(shf<0.62),False))&(hf>0.5)
    edge=ef>0.30
    ink=np.where(edge,~base|(hf<=0.5),base)
    # silhouette outline is always ink
    sil=(hf>0.5)&(ndi.binary_erosion(hf>0.5,iterations=1)==False)
    ink=np.where(sil,True,ink)&((hf>0.02))
    # anchors
    def proj(pt):
        qq=np.array(pt)@Rm.T; s=D/(D+qq[1]); return ((qq[0]*s*res+ox)/SS,(-qq[2]*s*res+oy)/SS)
    anch=dict(front=proj((0.0,0,0)),rear=proj((3.98,0,0)),face_top=proj((0.0,0,1.0)),face_bot=proj((0,0,-1.0)),
              face_l=proj((0,-1.0,0)),face_r=proj((0,1.0,0)))
    fill=ndi.binary_fill_holes(hf>0.5)
    return ink,anch,fill
if __name__=="__main__":
    import time; t=time.time()
    import sys; ink,a,_f=render((560,300),118,*[float(x) for x in sys.argv[1:4]] if len(sys.argv)>3 else ())
    print(time.time()-t,a)
    im=Image.fromarray(np.where(ink,0,255).astype(np.uint8)); im.save("e3d.png"); im.resize((1120,600),Image.NEAREST).save("e3d_x2.png")
