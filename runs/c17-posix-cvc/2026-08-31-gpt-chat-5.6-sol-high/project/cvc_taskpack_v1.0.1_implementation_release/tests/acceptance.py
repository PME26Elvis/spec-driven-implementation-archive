#!/usr/bin/env python3
import os, sys, subprocess, tempfile, shutil, hashlib, struct, fcntl, time, json, stat
from pathlib import Path

BIN=Path(sys.argv[1] if len(sys.argv)>1 else './cvc').resolve()
ROOT=BIN.parent
FAULT=ROOT/'tests/libfaultio.so'
RESULT={}
DETAIL={}

def run(args,cwd=None,env=None,ok=None,timeout=30,raw=False):
    e=os.environ.copy();
    if env:e.update({k:str(v) for k,v in env.items()})
    p=subprocess.run([str(BIN)]+list(args),cwd=cwd,env=e,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=timeout)
    if ok is not None and ((p.returncode==0)!=ok):
        raise AssertionError(f"{args}: rc={p.returncode}\nstdout={p.stdout!r}\nstderr={p.stderr!r}")
    if raw:return p
    return p.returncode,p.stdout.decode('utf-8','replace'),p.stderr.decode('utf-8','replace')

def mark(ids,fn):
    if isinstance(ids,str):ids=[ids]
    try:
        fn()
        for i in ids: RESULT[i]='PASS';DETAIL[i]=fn.__name__
    except Exception as ex:
        for i in ids: RESULT[i]='FAIL';DETAIL[i]=f"{fn.__name__}: {type(ex).__name__}: {ex}"

def mkrepo():
    t=tempfile.mkdtemp(prefix='cvc-acc-')
    run(['init'],t,ok=True)
    return t

def save(r,msg='m',ts=1,extra=None,ok=True):
    a=['save','-m',msg]+(extra or [])
    return run(a,r,{'CVC_TEST_TIMESTAMP':ts},ok=ok)

def refpath(r,name='main'):return Path(r)/'.cvc/refs/heads'/name

def head(r):
    name=(Path(r)/'.cvc/HEAD').read_text().strip().split('refs/heads/',1)[1]
    return refpath(r,name).read_text().strip()

def objpath(r,h):return Path(r)/'.cvc/objects'/h[:2]/h[2:]

def all_object_files(r):
    o=Path(r)/'.cvc/objects'
    return sorted([p for d in o.iterdir() if d.is_dir() and len(d.name)==2 for p in d.iterdir() if p.is_file() and len(p.name)==62])

def read_obj(r,h):
    b=objpath(r,h).read_bytes(); z=b.index(b'\0'); hdr=b[:z].decode('ascii'); typ,ln=hdr.split(' '); payload=b[z+1:]; assert str(len(payload))==ln;return typ,payload

def commit_info(r,h):
    typ,p=read_obj(r,h);assert typ=='commit'; root=p[:32].hex();pc=p[32];o=33;pars=[]
    for _ in range(pc):pars.append(p[o:o+32].hex());o+=32
    ts=struct.unpack('>q',p[o:o+8])[0];o+=8;ml=struct.unpack('>Q',p[o:o+8])[0];o+=8;msg=p[o:o+ml]
    return root,pars,ts,msg

def tree_flat(r,tree,prefix=''):
    typ,p=read_obj(r,tree);assert typ=='tree'; n=struct.unpack('>I',p[:4])[0];o=4;out={}
    prev=None
    for _ in range(n):
        ty=p[o];nl=struct.unpack('>I',p[o+1:o+5])[0];o+=5;name=p[o:o+nl];o+=nl;oid=p[o:o+32].hex();o+=32
        assert prev is None or prev<name;prev=name
        q=(prefix+'/'+name.decode('utf8')) if prefix else name.decode('utf8')
        if ty==3:out.update(tree_flat(r,oid,q))
        else:out[q]=(ty,oid)
    assert o==len(p);return out

def snapshot(r,h=None):
    if h is None:h=head(r)
    if not h:return {}
    return tree_flat(r,commit_info(r,h)[0])

def write_config(r,obj):
    (Path(r)/'.cvc/config.json').write_text(json.dumps(obj,ensure_ascii=False,separators=(',',':'))+'\n',encoding='utf8')

def raw_config(r,b): (Path(r)/'.cvc/config.json').write_bytes(b)

def branch_setup_conflict():
    r=mkrepo();Path(r,'f').write_text('one\ntwo\nthree\n');save(r,'base',1);run(['branch','create','dev'],r,ok=True);Path(r,'f').write_text('one\nOURS\nthree\n');save(r,'ours',2);run(['switch','dev'],r,ok=True);Path(r,'f').write_text('one\nTHEIRS\nthree\n');save(r,'theirs',3);run(['switch','main'],r,ok=True);return r

def make_loose(r,typ,payload):
    whole=typ.encode()+b' '+str(len(payload)).encode()+b'\0'+payload;h=hashlib.sha256(whole).hexdigest();p=objpath(r,h);p.parent.mkdir(exist_ok=True);p.write_bytes(whole);return h

def empty_tree(r):return make_loose(r,'tree',struct.pack('>I',0))

def commit_bytes(root_hex,parents=(),ts=0,msg=b'x'):
    return bytes.fromhex(root_hex)+bytes([len(parents)])+b''.join(bytes.fromhex(x) for x in parents)+struct.pack('>qQ',ts,len(msg))+msg

def assert_fail(args,r,**kw):return run(args,r,ok=False,**kw)

def test_A_build():
    p=subprocess.run(['make','clean'],cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=120)
    assert p.returncode==0,(p.stdout+p.stderr).decode()
    p=subprocess.run(['make','all','unit','tests/mergebase_unit','tests/libfaultio.so'],cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=120)
    assert p.returncode==0,(p.stdout+p.stderr).decode(); assert BIN.exists()
mark('A01',test_A_build)

def test_A_cli():
    p=run(['--help'],ok=True);assert all(x in p[1] for x in ['resolve','merge --continue','merge --abort','rollback','verify'])
    run(['wat'],ok=False)
    t=tempfile.mkdtemp();run(['status'],t,ok=False);shutil.rmtree(t)
    r=mkrepo();
    for bad in ['0','+1','01','x',str(2**64)]:run(['log','--max-count='+bad],r,ok=False)
    run(['log','--max-count=1'],r,ok=True)
    Path(r,'z').write_text('z');Path(r,'sub').mkdir();run(['status'],Path(r,'sub'),ok=True);assert 'z' in run(['status'],Path(r,'sub'),ok=True)[1]
    run(['status','--include=**','--include=*'],r,ok=False)
    for n in ['b','a','資料']:run(['branch','create',n],r,ok=True)
    out=run(['branch'],r,ok=True)[1];names=[x.split()[1] for x in out.splitlines() if len(x.split())>=2];assert names==sorted(names,key=lambda x:x.encode())
    shutil.rmtree(r)
mark(['A02','A03','A04','A05','A06','A07','A08','A09'],test_A_cli)

def test_B_init():
    r=tempfile.mkdtemp();run(['init'],r,ok=True);assert (Path(r)/'.cvc').is_dir();run(['init'],r,ok=False)
    assert (Path(r)/'.cvc/HEAD').read_bytes()==b'ref: refs/heads/main\n';assert refpath(r).read_bytes()==b'';assert (Path(r)/'.cvc/lock').stat().st_size==0
    out=run(['branch'],r,ok=True)[1];assert '* main (unborn)' in out;assert 'no commits' in run(['log'],r,ok=True)[1];run(['status'],r,ok=True)
    before=list(all_object_files(r));save(r,'empty',1,ok=True);assert head(r)=='' and all_object_files(r)==before
    shutil.rmtree(r)
mark(['B01','B02','B03','B04','B05','B06'],test_B_init)

def test_B_partial():
    parent=tempfile.mkdtemp();env={'LD_PRELOAD':FAULT,'FI_MODE':'init-state'};run(['init'],parent,env,ok=False);assert not (Path(parent)/'.cvc').exists();shutil.rmtree(parent)
mark('B07',test_B_partial)

# Config/parser black-box cases.
def cfg_case(raw,good):
    r=mkrepo();raw_config(r,raw if isinstance(raw,bytes) else raw.encode());run(['config','validate'],r,ok=good);shutil.rmtree(r)
mark('C01',lambda: cfg_case(b'{"format_version":1}\n',True))
mark('C02',lambda: cfg_case('{"format_version":1,"save":{"show_diffstat":false},"tracking":{"include":["**","資料/**"],"exclude":["*.tmp"]},"diffstat":{"include":["*.c"],"exclude":[]}}',True))
mark('C03',lambda: cfg_case('{"format_version":1,"tracking":{"include":["**"],"exclude":[]},"diffstat":{"include":[],"exclude":["x"]}}',True))
mark('C04',lambda: cfg_case(r'{"format_version":1,"tracking":{"include":["a\\\"b","a\\\\b","a\\/b"],"exclude":[]}}',True))
mark('C05',lambda: cfg_case(r'{"format_version":1,"tracking":{"include":["\u4e2d"],"exclude":[]}}',True))
mark('C06',lambda: cfg_case(r'{"format_version":1,"tracking":{"include":["\ud83d\ude00"],"exclude":[]}}',True))
mark('C07',lambda: cfg_case(r'{"format_version":1,"tracking":{"include":["\ud83d"],"exclude":[]}}',False))
mark('C08',lambda: cfg_case(b'{"format_version":1,"tracking":{"include":["\xff"],"exclude":[]}}',False))
mark('C09',lambda: (cfg_case(r'{"format_version":1,"format_version":1}',False),cfg_case(r'{"format_version":1,"tracking":{"include":[],"\u0069nclude":[]}}',False)))
mark('C10',lambda: cfg_case('{"format_version":1,}',False))
mark('C11',lambda: cfg_case('{/*x*/"format_version":1}',False))
mark('C12',lambda: cfg_case('{"format_version":1}x',False))
mark('C13',lambda: cfg_case('{"format_version":1e99999}',False))
mark('C14',lambda: (cfg_case('{"format_version":1,"wat":1}',False),cfg_case('{"format_version":1,"save":{"wat":1}}',False)))
mark('C15',lambda: cfg_case('{"format_version":1,"tracking":{"include":true}}',False))
mark('C16',lambda: cfg_case(b'\xef\xbb\xbf{"format_version":1}',False))
mark('C17',lambda: cfg_case(r'{"format_version":1,"tracking":{"include":["a\u0000b"],"exclude":[]}}',False))
mark('C18',lambda: (cfg_case('{"format_version":1.0}',False),cfg_case('{"format_version":1e0}',False)))
def c19():
    r=mkrepo();run(['config','show'],r,ok=True);run(['config','validate'],r,ok=True);raw_config(r,b'{');run(['config','validate'],r,ok=False);run(['status'],r,ok=False);shutil.rmtree(r)
mark('C19',c19)
mark('C20',lambda: [cfg_case(x,False) for x in ['{"format_version":01}','{"format_version":1.}','{"format_version":1e}']])

# Unit algorithm coverage is executed again so these acceptance IDs have an automated dependency.
def unit_ok():
    p=subprocess.run([str(ROOT/'tests/unit_core')],cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.PIPE);assert p.returncode==0,(p.stdout+p.stderr).decode()
mark(['D01','D02','D03','D04','D10','D13','H01','H02','H03','H04','H10','H11','I01','I02','I03','I04','I05','I06','I07','I08','I09','I10'],unit_ok)

def d05_06():
    r=mkrepo();Path(r,'a').write_text('same');Path(r,'b').write_text('same');save(r,'x',1);bl=[p for p in all_object_files(r) if read_obj(r,p.parent.name+p.name)[0]=='blob'];assert len(bl)==1
    Path(r,'a').write_text('new');save(r,'y',2);n=len([p for p in all_object_files(r) if read_obj(r,p.parent.name+p.name)[0]=='blob']);Path(r,'a').write_text('same');save(r,'z',3);n2=len([p for p in all_object_files(r) if read_obj(r,p.parent.name+p.name)[0]=='blob']);assert n2==n;shutil.rmtree(r)
mark(['D05','D06'],d05_06)
def d07():
    r=mkrepo();Path(r,'a').write_text('same');save(r,'x',1);h=snapshot(r)['a'][1];objpath(r,h).write_bytes(b'broken');old=head(r);Path(r,'b').write_text('b');save(r,'y',2,ok=False);assert head(r)==old;shutil.rmtree(r)
mark('D07',d07)
def deterministic():
    rs=[]
    for order in [('b','a'),('a','b')]:
        r=mkrepo();
        for x in order:Path(r,x).write_text(x)
        save(r,'same',123);rs.append((r,head(r),commit_info(r,head(r))[0]))
    assert rs[0][1]==rs[1][1] and rs[0][2]==rs[1][2]
    for r,_,__ in rs:shutil.rmtree(r)
mark(['D08','D09'],deterministic)
def d11():
    r=mkrepo();Path(r,'z').write_text('z');Path(r,'a').write_text('a');save(r,'x',1);root=commit_info(r,head(r))[0];typ,p=read_obj(r,root);assert typ=='tree' and p[:4]==struct.pack('>I',2);o=4;names=[]
    for _ in range(2):ty=p[o];nl=struct.unpack('>I',p[o+1:o+5])[0];o+=5;names.append(p[o:o+nl]);o+=nl+32
    assert names==[b'a',b'z'] and o==len(p);shutil.rmtree(r)
mark('D11',d11)
def d12():
    r=mkrepo();et=empty_tree(r);blob=make_loose(r,'blob',b'x');# unsorted tree z then a
    def ent(n):return bytes([1])+struct.pack('>I',1)+n+bytes.fromhex(blob)
    make_loose(r,'tree',struct.pack('>I',2)+ent(b'z')+ent(b'a'));run(['verify'],r,ok=False);shutil.rmtree(r)
mark('D12',d12)
def d14():
    for ts in ['+1','01','-0',' 1',str(2**63)]:
        r=mkrepo();Path(r,'a').write_text('a');old=head(r);run(['save','-m','x'],r,{'CVC_TEST_TIMESTAMP':ts},ok=False);assert head(r)==old;shutil.rmtree(r)
mark('D14',d14)

# Eligibility/scanning.
def e_basic():
    r=mkrepo();Path(r,'t').write_text('hello');Path(r,'empty').write_bytes(b'');Path(r,'.env').write_text('x');Path(r,'中文').write_text('中');Path(r,'d/e/f').mkdir(parents=True);Path(r,'d/e/f/x').write_text('x');save(r,'x',1);s=snapshot(r);assert all(x in s for x in ['t','empty','.env','中文','d/e/f/x']);Path(r,'onlyempty').mkdir();assert 'working tree clean' in run(['status'],r,ok=True)[1];shutil.rmtree(r)
mark(['E01','E02','E09','E10','E11','E13'],e_basic)
def e_nul():
    r=mkrepo();Path(r,'n0').write_bytes(b'\0x');Path(r,'n8191').write_bytes(b'a'*8191+b'\0');Path(r,'n8192').write_bytes(b'a'*8192+b'\0tail');out=run(['status'],r,ok=True)[1];assert 'n8192' in out and 'n0' not in out and 'n8191' not in out;save(r,'x',1);assert 'n8192' in snapshot(r);shutil.rmtree(r)
mark(['E03','E04','E05'],e_nul)
def e_size():
    r=mkrepo();
    with open(Path(r,'ok'),'wb') as f:f.write(b'a'*8192);f.truncate(8388608)
    with open(Path(r,'bad'),'wb') as f:f.write(b'a'*8192);f.truncate(8388609)
    out=run(['status'],r,ok=True,timeout=60)[1];assert 'ok' in out and 'bad' not in out;shutil.rmtree(r)
mark('E06',e_size)
def e_fifo_nested():
    r=mkrepo();os.mkfifo(Path(r,'pipe'));Path(r,'nested/.cvc').mkdir(parents=True);Path(r,'nested/x').write_text('x');out=run(['status'],r,ok=True,timeout=5)[1];assert 'pipe' not in out and 'nested/x' not in out and 'ignored:' in out;shutil.rmtree(r)
mark(['E07','E08'],e_fifo_nested)
def e_many():
    r=mkrepo();d=Path(r,'many');d.mkdir();
    for i in range(2000):(d/f'f{i:04d}').write_text(str(i))
    save(r,'many',1,ok=True);assert len(snapshot(r))==2000;shutil.rmtree(r)
mark('E12',e_many)
def e_roundtrip_nul():
    r=mkrepo();data=b'a'*8192+b'\0ZZ\n';Path(r,'x').write_bytes(data);save(r,'x',1);Path(r,'x').write_bytes(b'changed');run(['restore','x','--from','main'],r,ok=True);assert Path(r,'x').read_bytes()==data;shutil.rmtree(r)
mark('E14',e_roundtrip_nul)
def e_badname():
    r=mkrepo();fd=os.open(os.fsencode(r)+b'/bad\xff',os.O_CREAT|os.O_WRONLY,0o666);os.write(fd,b'x');os.close(fd);Path(r,'bad\x01name').write_text('x');p=run(['status'],r,ok=True);assert 'ignored:' in p[1] and ('unsupported filename' in p[2]);save(r,'x',1);assert not snapshot(r);shutil.rmtree(r)
mark('E15',e_badname)

# Symlink cases.
def symlink_round(target,case):
    r=mkrepo();os.symlink(target,Path(r,'l'));save(r,'s',1);os.unlink(Path(r,'l'));os.symlink('changed',Path(r,'l'));run(['restore','l','--from','main'],r,ok=True);assert os.readlink(Path(r,'l'))==target;shutil.rmtree(r)
mark('F01',lambda:symlink_round('../rel','F01'));mark('F02',lambda:symlink_round('/tmp/absolute-target','F02'));mark('F03',lambda:symlink_round('definitely-missing','F03'))
def f04_05():
    r=mkrepo();Path(r,'real').mkdir();Path(r,'real/x').write_text('x');os.symlink('real',Path(r,'ld'));os.symlink('loop',Path(r,'loop'));save(r,'s',1);s=snapshot(r);assert 'ld' in s and 'ld/x' not in s and 'loop' in s;shutil.rmtree(r)
mark(['F04','F05'],f04_05)
def f06_07():
    r=mkrepo();os.symlink('a',Path(r,'l'));Path(r,'f').write_text('x');save(r,'s',1);os.unlink(Path(r,'l'));os.symlink('b',Path(r,'l'));os.unlink(Path(r,'f'));os.symlink('x',Path(r,'f'));o=run(['status'],r,ok=True)[1];assert 'modified     l' in o and 'type-changed f' in o;shutil.rmtree(r)
mark(['F06','F07'],f06_07)
def f08():
    r=mkrepo();run(['branch','create','dev'],r,ok=True);run(['switch','dev'],r,ok=True);Path(r,'d').mkdir();Path(r,'d/x').write_text('repo');save(r,'dev',1);run(['switch','main'],r,ok=True);outside=tempfile.mkdtemp();os.symlink(outside,Path(r,'d'));run(['switch','dev'],r,ok=False);assert not Path(outside,'x').exists();shutil.rmtree(outside);shutil.rmtree(r)
mark('F08',f08)

# Save/status/filter behavior.
def g_core():
    r=mkrepo();Path(r,'a').write_text('a\n');save(r,'first',1);h1=head(r);assert len(commit_info(r,h1)[1])==0;save(r,'noop',2);assert head(r)==h1
    Path(r,'a').write_text('A\n');Path(r,'b').write_text('b');Path(r,'c').write_text('c');save(r,'mid',3);os.unlink(Path(r,'c'));Path(r,'b').write_text('B');Path(r,'d').write_text('d');o=run(['status'],r,ok=True)[1];assert all(x in o for x in ['modified     b','deleted      c','added        d']);shutil.rmtree(r)
mark(['G01','G02','G03'],g_core)
def g04_05():
    r=mkrepo();Path(r,'x').write_text('x');save(r,'x',1);os.unlink(Path(r,'x'));os.symlink('q',Path(r,'x'));os.mkfifo(Path(r,'p'));o=run(['status'],r,ok=True)[1];assert 'type-changed x' in o and 'ignored:' in o;shutil.rmtree(r)
mark(['G04','G05'],g04_05)
def g06():
    r=mkrepo();Path(r,'a').write_text('a');Path(r,'b').write_text('b');save(r,'all',1,extra=['--include=a']);assert set(snapshot(r))=={'a','b'};shutil.rmtree(r)
mark('G06',g06)
def g07():
    r=mkrepo();Path(r,'a').write_text('a');save(r,'a',1);Path(r,'a').write_bytes(b'\0bin');save(r,'drop',2);assert 'a' not in snapshot(r) and Path(r,'a').exists();shutil.rmtree(r)
mark('G07',g07)
def g08():
    r=mkrepo();Path(r,'a').write_text('a');save(r,'a',1);write_config(r,{'format_version':1,'tracking':{'include':['**'],'exclude':['a']}});save(r,'filter',2);assert 'a' not in snapshot(r) and Path(r,'a').exists();shutil.rmtree(r)
mark('G08',g08)
def g09():
    r=mkrepo();Path(r,'a').write_text('a');m='訊息😀'*500;save(r,m,1);assert m in run(['log','--max-count=1'],r,ok=True)[1];shutil.rmtree(r)
mark('G09',g09)
def g10():
    r=mkrepo();write_config(r,{'format_version':1,'save':{'show_diffstat':False}});Path(r,'a').write_text('a\n');o=save(r,'x',1)[1];assert 'total:' not in o and 'a' in snapshot(r);shutil.rmtree(r)
mark('G10',g10)
mark('G11',lambda:(lambda r:(run(['save','-m',''],r,ok=False),shutil.rmtree(r)))(mkrepo()))

def h05():
    r=mkrepo();write_config(r,{'format_version':1,'tracking':{'include':['**'],'exclude':['*.tmp']}});Path(r,'a').write_text('a');Path(r,'b.tmp').write_text('b');o=run(['status'],r,ok=True)[1];assert 'a' in o and 'b.tmp' not in o;shutil.rmtree(r)
mark('H05',h05)
def h06_09_12():
    r=mkrepo();Path(r,'a').write_text('a\n');Path(r,'b').write_text('b\n');o=save(r,'x',1,extra=['--include=a'])[1];assert set(snapshot(r))=={'a','b'} and ' a |' in o and ' b |' not in o;shutil.rmtree(r)
mark(['H06','H09','H12'],h06_09_12)
mark('H07',lambda:(lambda r:(run(['status','--include=a,,b'],r,ok=False),shutil.rmtree(r)))(mkrepo()))
def h08():
    r=mkrepo();write_config(r,{'format_version':1,'tracking':{'include':[],'exclude':[]},'diffstat':{'include':[],'exclude':[]}});Path(r,'a').write_text('a');assert 'working tree clean' in run(['status'],r,ok=True)[1];run(['status','--include='],r,ok=False);shutil.rmtree(r)
mark('H08',h08)
def h13():
    r=mkrepo();write_config(r,{'format_version':1,'diffstat':{'include':['nope'],'exclude':['**']}});Path(r,'a').write_text('a');assert 'a' in run(['status'],r,ok=True)[1] and 'a' in run(['diff'],r,ok=True)[1];shutil.rmtree(r)
mark('H13',h13)
def h14():
    r=mkrepo();Path(r,'a').write_text('a');assert not any(line.rstrip().endswith(' a') for line in run(['status','--include= a'],r,ok=True)[1].splitlines());run(['status','--include=a,'],r,ok=False);run(['status','--include=,a'],r,ok=False);shutil.rmtree(r)
mark('H14',h14)
def i11_12():
    r=mkrepo();Path(r,'a').write_text('x\ny\n');o=save(r,'a',1)[1];assert '+2 -0' in o;Path(r,'a').write_text('x\nz\n');o=save(r,'b',2)[1];assert '+1 -1' in o;os.unlink(Path(r,'a'));o=save(r,'c',3)[1];assert '+0 -2' in o;shutil.rmtree(r)
mark(['I11','I12'],i11_12)
def i13():
    r=mkrepo();Path(r,'x').write_bytes(b'a'*8192+b'\0old\n');save(r,'a',1);Path(r,'x').write_bytes(b'a'*8192+b'\0new\n');p=run(['diff'],r,ok=True,raw=True);assert b'old' in p.stdout and b'new' in p.stdout;shutil.rmtree(r)
mark('I13',i13)

# Branch/switch.
def j01_05_06():
    r=mkrepo();run(['branch','create','dev'],r,ok=True);o=run(['branch'],r,ok=True)[1];assert 'dev' in o and '* main' in o;run(['branch','create','dev'],r,ok=False);run(['branch','delete','main'],r,ok=False);shutil.rmtree(r)
mark(['J01','J02','J04','J05','J06'],j01_05_06)
def j03():
    r=mkrepo();Path(r,'x').write_text('m');save(r,'m',1);run(['branch','create','d'],r,ok=True);run(['switch','d'],r,ok=True);Path(r,'x').write_text('d');save(r,'d',2);run(['switch','main'],r,ok=True);assert Path(r,'x').read_text()=='m';run(['switch','d'],r,ok=True);assert Path(r,'x').read_text()=='d';shutil.rmtree(r)
mark('J03',j03)
def j07_12():
    r=mkrepo();run(['branch','create','d'],r,ok=True);Path(r,'new').write_text('x');run(['switch','d'],r,ok=False);run(['switch','main'],r,ok=True);assert Path(r,'new').exists();shutil.rmtree(r)
mark(['J07','J12'],j07_12)
def collision(filtered):
    r=mkrepo();run(['branch','create','d'],r,ok=True);run(['switch','d'],r,ok=True);Path(r,'x').write_text('tracked');save(r,'d',1);run(['switch','main'],r,ok=True)
    if filtered:write_config(r,{'format_version':1,'tracking':{'include':['**'],'exclude':['x']}});Path(r,'x').write_text('local')
    else:Path(r,'x').write_bytes(b'\0local')
    run(['switch','d'],r,ok=False);assert Path(r,'x').read_bytes().endswith(b'local');shutil.rmtree(r)
mark('J08',lambda:collision(True));mark('J09',lambda:collision(False))
def j10():
    r=mkrepo();run(['branch','create','d'],r,ok=True);run(['switch','d'],r,ok=True);Path(r,'dir').mkdir();Path(r,'dir/tracked').write_text('t');save(r,'d',1);run(['switch','main'],r,ok=True);write_config(r,{'format_version':1,'tracking':{'include':['**'],'exclude':['**/*.tmp']}});Path(r,'dir').mkdir(exist_ok=True);Path(r,'dir/local.tmp').write_text('keep');run(['switch','d'],r,ok=True);assert Path(r,'dir/local.tmp').read_text()=='keep' and Path(r,'dir/tracked').read_text()=='t';shutil.rmtree(r)
mark('J10',j10)
def j11():
    r=mkrepo();run(['branch','create','foo'],r,ok=True);run(['branch','create','foo/bar'],r,ok=False);run(['branch','delete','foo'],r,ok=True);(Path(r)/'.cvc/refs/heads/foo').mkdir();run(['branch','create','foo'],r,ok=True);shutil.rmtree(r)
mark('J11',j11)
mark('J11b',lambda:(lambda r:(run(['branch','create','-x'],r,ok=False),shutil.rmtree(r)))(mkrepo()))
def j13():
    r=mkrepo();Path(r,'x').write_text('x');save(r,'m',1);run(['branch','create','d'],r,ok=True);run(['switch','d'],r,ok=True);Path(r,'u').write_text('u');save(r,'u',2);run(['switch','main'],r,ok=True);p=run(['branch','delete','d'],r,ok=True);assert 'warning' in p[2].lower() and not refpath(r,'d').exists();run(['verify'],r,ok=True);shutil.rmtree(r)
mark('J13',j13)
def j14():
    r=mkrepo();bad=['.','..','a..b','/a','a/','a//b','a.','a\\b','HEAD','-'+'x'*130]
    for x in bad:run(['branch','create',x],r,ok=False)
    shutil.rmtree(r)
mark('J14',j14)

# Revision + restore.
def k01_03_05_07():
    r=mkrepo();Path(r,'d').mkdir();Path(r,'d/a').write_text('old');Path(r,'x').write_text('old');save(r,'base',1);h=head(r);Path(r,'d/a').write_text('new');Path(r,'d/b').write_text('b');Path(r,'x').write_text('new');save(r,'new',2);run(['restore','x','--from',h],r,ok=True);assert Path(r,'x').read_text()=='old';run(['restore','d','--from',h[:8]],r,ok=True);assert Path(r,'d/a').read_text()=='old' and not Path(r,'d/b').exists();run(['restore','x','--from','main'],r,ok=True);assert Path(r,'x').read_text()=='new';shutil.rmtree(r)
mark(['K01','K02','K03','K05','K07'],k01_03_05_07)
def k04():
    r=mkrepo();root=empty_tree(r);seen={};pair=None
    for i in range(200000):
        pl=commit_bytes(root,(),i,f'm{i}'.encode());whole=b'commit '+str(len(pl)).encode()+b'\0'+pl;h=hashlib.sha256(whole).hexdigest();pfx=h[:8]
        if pfx in seen and seen[pfx][0]!=h:pair=(seen[pfx],(h,whole));break
        seen[pfx]=(h,whole)
    assert pair
    for h,w in pair:p=objpath(r,h);p.parent.mkdir(exist_ok=True);p.write_bytes(w)
    run(['diff',pair[0][0][:8]],r,ok=False);shutil.rmtree(r)
mark('K04',k04)
def k06():
    r=mkrepo();os.symlink('a',Path(r,'l'));save(r,'s',1);h=head(r);os.unlink(Path(r,'l'));os.symlink('b',Path(r,'l'));run(['restore','l','--from',h],r,ok=True);assert os.readlink(Path(r,'l'))=='a';shutil.rmtree(r)
mark('K06',k06)
def k08_09():
    r=mkrepo();Path(r,'d').mkdir();Path(r,'d/a').write_text('a');save(r,'base',1);h=head(r);Path(r,'d/b').write_text('b');save(r,'new',2);Path(r,'d/local').write_text('keep');run(['restore','d','--from',h],r,ok=True);assert not Path(r,'d/b').exists() and Path(r,'d/local').read_text()=='keep';before=Path(r,'d/a').read_bytes();run(['restore','nope','--from',h],r,ok=False);assert Path(r,'d/a').read_bytes()==before;shutil.rmtree(r)
mark(['K08','K09'],k08_09)
def k10():
    r=mkrepo();Path(r,'d').mkdir();Path(r,'d/a').write_text('a');save(r,'base',1);h=head(r);Path(r,'d/a').unlink();Path(r,'d').rmdir();save(r,'remove-d',2);Path(r,'d').write_text('untracked-shape');before=Path(r,'d').read_bytes();run(['restore','d','--from',h],r,ok=False);assert Path(r,'d').read_bytes()==before;shutil.rmtree(r)
mark('K10',k10)
def k11():
    r=mkrepo();Path(r,'a').write_text('a');save(r,'base',1);root=empty_tree(r);pl=commit_bytes(root,(),44,b'unreachable');uh=make_loose(r,'commit',pl);run(['diff',uh],r,ok=True);run(['diff',uh[:8]],r,ok=True);bad='deadbeef'+'0'*56;p=objpath(r,bad);p.parent.mkdir(exist_ok=True);p.write_bytes(b'corrupt');run(['diff','deadbeef'],r,ok=False);shutil.rmtree(r)
mark('K11',k11)
def k12():
    r=mkrepo();Path(r,'-notes').write_text('old');Path(r,'root').write_text('old');save(r,'b',1);h=head(r);Path(r,'-notes').write_text('new');Path(r,'root').write_text('new');Path(r,'sub').mkdir();run(['restore','root','--from',h],Path(r,'sub'),ok=True);run(['restore','-notes','--from',h],Path(r,'sub'),ok=True);assert Path(r,'root').read_text()=='old' and Path(r,'-notes').read_text()=='old';shutil.rmtree(r)
mark('K12',k12)
def k13():
    r=mkrepo();Path(r,'x').write_text('x');save(r,'x',1)
    for p in ['/x','.','..','a/../b','a//b','a/./b','bad\x01x']:run(['restore',p,'--from','main'],r,ok=False)
    for p in ['/x','.','..','a/../b','a//b','a/./b','bad\x01x']:run(['resolve',p],r,ok=False)
    shutil.rmtree(r)
mark('K13',k13)

# Merge helpers/cases.
def l_self_ancestor_ff():
    r=mkrepo();Path(r,'x').write_text('base');save(r,'base',1);run(['branch','create','old'],r,ok=True);Path(r,'x').write_text('new');save(r,'new',2);Path(r,'dirty').write_text('dirty');h=head(r);run(['merge','main'],r,ok=True);assert head(r)==h and Path(r,'dirty').exists();Path(r,'dirty').unlink();run(['merge','old'],r,ok=True);assert head(r)==h;run(['switch','old'],r,ok=True);run(['merge','main'],r,ok=True);assert head(r)==h and Path(r,'x').read_text()=='new';shutil.rmtree(r)
mark(['L01','L02','L03'],l_self_ancestor_ff)

def l04():
    r=mkrepo();Path(r,'base').write_text('b');save(r,'b',1);run(['branch','create','d'],r,ok=True);Path(r,'ours').write_text('o');save(r,'o',2);run(['switch','d'],r,ok=True);Path(r,'theirs').write_text('t');save(r,'t',3);th=head(r);run(['switch','main'],r,ok=True);oh=head(r);run(['merge','d'],r,{'CVC_TEST_TIMESTAMP':4},ok=True);assert Path(r,'ours').exists() and Path(r,'theirs').exists();pars=commit_info(r,head(r))[1];assert pars==[oh,th];shutil.rmtree(r)
mark('L04',l04)

def samefile(ours,theirs,expected,ok=True):
    r=mkrepo();Path(r,'f').write_text('a\nb\nc\n');save(r,'b',1);run(['branch','create','d'],r,ok=True);Path(r,'f').write_text(ours);Path(r,'uo').write_text('o');save(r,'o',2);run(['switch','d'],r,ok=True);Path(r,'f').write_text(theirs);Path(r,'ut').write_text('t');save(r,'t',3);run(['switch','main'],r,ok=True);p=run(['merge','d'],r,{'CVC_TEST_TIMESTAMP':4},ok=ok);return r,p

def l05():
    r,p=samefile('A\nb\nc\n','a\nb\nC\n','A\nb\nC\n',True);assert Path(r,'f').read_text()=='A\nb\nC\n';shutil.rmtree(r)
mark('L05',l05)
def l06():
    r,p=samefile('a\nX\nc\n','a\nX\nc\n','a\nX\nc\n',True);assert Path(r,'f').read_text()=='a\nX\nc\n';shutil.rmtree(r)
    r=mkrepo();Path(r,'f').write_text('a\n');save(r,'b',1);run(['branch','create','d'],r,ok=True);Path(r,'f').write_text('I\na\n');Path(r,'o').write_text('1');save(r,'o',2);run(['switch','d'],r,ok=True);Path(r,'f').write_text('I\na\n');Path(r,'t').write_text('1');save(r,'t',3);run(['switch','main'],r,ok=True);run(['merge','d'],r,{'CVC_TEST_TIMESTAMP':4},ok=True);assert Path(r,'f').read_text()=='I\na\n';shutil.rmtree(r)
mark('L06',l06)
def l07():
    r,p=samefile('a\nX\nc\n','a\nY\nc\n','',False);assert b'<<<<<<< ours' in Path(r,'f').read_bytes() and (Path(r)/'.cvc/state/merge').exists();shutil.rmtree(r)
    r=mkrepo();Path(r,'f').write_text('a\n');save(r,'b',1);run(['branch','create','d'],r,ok=True);Path(r,'f').write_text('X\na\n');save(r,'o',2);run(['switch','d'],r,ok=True);Path(r,'f').write_text('Y\na\n');save(r,'t',3);run(['switch','main'],r,ok=True);run(['merge','d'],r,ok=False);assert b'<<<<<<<' in Path(r,'f').read_bytes();shutil.rmtree(r)
mark('L07',l07)
def l07b():
    r=mkrepo();Path(r,'f').write_text('a\nb\nc\n');save(r,'b',1);run(['branch','create','d'],r,ok=True);Path(r,'f').write_text('a\nI\nb\nc\n');Path(r,'o').write_text('1');save(r,'o',2);run(['switch','d'],r,ok=True);Path(r,'f').write_text('a\nB\nc\n');Path(r,'t').write_text('1');save(r,'t',3);run(['switch','main'],r,ok=True);run(['merge','d'],r,{'CVC_TEST_TIMESTAMP':4},ok=True);assert Path(r,'f').read_text()=='a\nI\nB\nc\n';shutil.rmtree(r)
mark('L07b',l07b)
def l08():
    r=mkrepo();Path(r,'f').write_text('b');save(r,'b',1);run(['branch','create','d'],r,ok=True);Path(r,'f').unlink();save(r,'del',2);run(['switch','d'],r,ok=True);Path(r,'f').write_text('mod');save(r,'mod',3);run(['switch','main'],r,ok=True);run(['merge','d'],r,ok=False);assert not Path(r,'f').exists() and not list(Path(r).glob('f.*'));shutil.rmtree(r)
mark('L08',l08)
def l09():
    r=mkrepo();Path(r,'seed').write_text('s');save(r,'b',1);run(['branch','create','d'],r,ok=True);Path(r,'f').write_text('O');save(r,'o',2);run(['switch','d'],r,ok=True);Path(r,'f').write_text('T');save(r,'t',3);run(['switch','main'],r,ok=True);run(['merge','d'],r,ok=False);assert b'<<<<<<<' in Path(r,'f').read_bytes();shutil.rmtree(r)
mark('L09',l09)
def l10():
    r=mkrepo();Path(r,'f').write_text('b');save(r,'b',1);run(['branch','create','d'],r,ok=True);Path(r,'f').write_text('O');save(r,'o',2);run(['switch','d'],r,ok=True);Path(r,'f').unlink();os.symlink('T',Path(r,'f'));save(r,'t',3);run(['switch','main'],r,ok=True);run(['merge','d'],r,ok=False);assert Path(r,'f').is_file() and Path(r,'f').read_text()=='O';shutil.rmtree(r)
mark('L10',l10)
def l11():
    r=mkrepo();Path(r,'a').write_text('a');save(r,'b',1);run(['branch','create','d'],r,ok=True);Path(r,'dirty').write_text('x');run(['merge','d'],r,ok=False);shutil.rmtree(r)
mark('L11',l11)
def l12_13_18():
    r=branch_setup_conflict();run(['merge','dev'],r,ok=False);Path(r,'f').write_text('resolved\n');run(['resolve','f'],r,ok=True)
    for cmd in [['save','-m','x'],['branch','create','x'],['branch','delete','dev'],['switch','dev'],['rollback','dev','-m','x'],['merge','dev']]:run(cmd,r,ok=False)
    Path(r,'f').write_text('changed after resolve\n');run(['merge','--continue'],r,{'CVC_TEST_TIMESTAMP':4},ok=False);run(['resolve','f'],r,ok=True);ours=head(r);theirs=refpath(r,'dev').read_text().strip();run(['merge','--continue'],r,{'CVC_TEST_TIMESTAMP':4},ok=True);assert commit_info(r,head(r))[1]==[ours,theirs];shutil.rmtree(r)
mark(['L12','L13','L18'],l12_13_18)
def l14():
    r=mkrepo();Path(r,'f').write_text('b\n');Path(r,'g').write_text('base\n');save(r,'b',1);run(['branch','create','d'],r,ok=True);Path(r,'f').write_text('O\n');save(r,'o',2);run(['switch','d'],r,ok=True);Path(r,'f').write_text('T\n');Path(r,'g').write_text('theirs\n');save(r,'t',3);run(['switch','main'],r,ok=True);run(['merge','d'],r,ok=False);Path(r,'f').write_text('R\n');run(['resolve','f'],r,ok=True);Path(r,'g').write_text('tampered\n');run(['merge','--continue'],r,{'CVC_TEST_TIMESTAMP':4},ok=False);shutil.rmtree(r)
mark('L14',l14)
def l15():
    r=branch_setup_conflict();before=Path(r,'f').read_bytes();oh=head(r);run(['merge','dev'],r,ok=False);run(['merge','--abort'],r,ok=True);assert Path(r,'f').read_bytes()==before and head(r)==oh and not (Path(r)/'.cvc/state/merge').exists();shutil.rmtree(r)
mark('L15',l15)
def l16():
    r=mkrepo();Path(r,'base').write_text('b');save(r,'b',1);run(['branch','create','side'],r,ok=True);Path(r,'m1').write_text('m');save(r,'m1',2);run(['switch','side'],r,ok=True);Path(r,'s1').write_text('s');save(r,'s1',3);run(['switch','main'],r,ok=True);run(['merge','side'],r,{'CVC_TEST_TIMESTAMP':4},ok=True);run(['branch','create','later'],r,ok=True);run(['switch','side'],r,ok=True);Path(r,'s2').write_text('s2');save(r,'s2',5);run(['switch','later'],r,ok=True);Path(r,'l2').write_text('l2');save(r,'l2',6);run(['merge','side'],r,{'CVC_TEST_TIMESTAMP':7},ok=True);assert Path(r,'s2').exists() and Path(r,'l2').exists();shutil.rmtree(r)
mark('L16',l16)
def l17():
    r=mkrepo();Path(r,'base').write_text('b');save(r,'b',1);run(['branch','create','d'],r,ok=True);Path(r,'new').mkdir();Path(r,'new/o').write_text('o');save(r,'o',2);run(['switch','d'],r,ok=True);Path(r,'new').mkdir();Path(r,'new/t').write_text('t');save(r,'t',3);run(['switch','main'],r,ok=True);run(['merge','d'],r,{'CVC_TEST_TIMESTAMP':4},ok=True);assert Path(r,'new/o').exists() and Path(r,'new/t').exists();shutil.rmtree(r)
mark('L17',l17)
def l19():
    r=branch_setup_conflict();ours=head(r);theirs=refpath(r,'dev').read_text().strip();run(['merge','dev'],r,ok=False);refpath(r).write_text(theirs+'\n');run(['resolve','f'],r,ok=False);run(['merge','--continue'],r,ok=False);assert refpath(r).read_text().strip()==theirs and refpath(r).read_text().strip()!=ours;shutil.rmtree(r)
mark('L19',l19)
def l20():
    r=mkrepo();Path(r,'dirty').write_text('x');run(['merge','main'],r,ok=True);run(['branch','create','u'],r,ok=True);run(['merge','u'],r,ok=True);Path(r,'dirty').unlink();Path(r,'a').write_text('a');save(r,'born',1);bh=head(r);run(['merge','u'],r,ok=True);assert head(r)==bh;run(['switch','u'],r,ok=True);assert head(r)=='';run(['merge','main'],r,ok=True);assert head(r)==bh and Path(r,'a').read_text()=='a';shutil.rmtree(r)
mark('L20',l20)
def l21():
    r=mkrepo();mid=b'x'*(8388608-7);base=b'A\n'+mid+b'\nZ\n';assert len(base)==8388606;Path(r,'f').write_bytes(base);save(r,'b',1);run(['branch','create','d'],r,ok=True);Path(r,'f').write_bytes(b'O\n'+base);assert Path(r,'f').stat().st_size==8388608;save(r,'o',2);run(['switch','d'],r,ok=True);Path(r,'f').write_bytes(base+b'T\n');assert Path(r,'f').stat().st_size==8388608;save(r,'t',3);run(['switch','main'],r,ok=True);run(['merge','d'],r,ok=False,timeout=120);assert (Path(r)/'.cvc/state/merge').exists();shutil.rmtree(r)
mark('L21',l21)
def finalizing_repo():
    r=branch_setup_conflict();run(['merge','dev'],r,ok=False);Path(r,'f').write_text('R\n');run(['resolve','f'],r,ok=True);env={'CVC_TEST_TIMESTAMP':4,'LD_PRELOAD':FAULT,'FI_MODE':'ref-main'};run(['merge','--continue'],r,env,ok=False);sp=Path(r)/'.cvc/state/merge';assert sp.exists();intended=sp.read_bytes()[-32:].hex();assert objpath(r,intended).exists() and head(r)!=intended;return r,intended

def l22():
    r,intended=finalizing_repo();run(['resolve','f'],r,ok=False);run(['restore','f','--from','main'],r,ok=False);run(['merge','dev'],r,ok=False);sp=Path(r)/'.cvc/state/merge';run(['status'],r,ok=True);assert sp.exists();refpath(r).write_text(intended+'\n');o=run(['status'],r,ok=True)[1];assert 'cleanup pending' in o and sp.exists();run(['branch','create','cleaned'],r,ok=True);assert not sp.exists();shutil.rmtree(r)
mark('L22',l22)
def l23():
    r,intended=finalizing_repo();objs={str(p) for p in all_object_files(r)};run(['merge','--continue','-m','different'],r,ok=False);assert (Path(r)/'.cvc/state/merge').exists();run(['merge','--continue'],r,{'CVC_TEST_TIMESTAMP':999},ok=True);assert head(r)==intended and {str(p) for p in all_object_files(r)}==objs;shutil.rmtree(r)
    r,intended=finalizing_repo();refpath(r).write_text(intended+'\n');run(['merge','--continue'],r,{'CVC_TEST_TIMESTAMP':888},ok=True);assert head(r)==intended and not (Path(r)/'.cvc/state/merge').exists();shutil.rmtree(r)
mark('L23',l23)
# L24 is covered by a dedicated production merge-base unit appended below and invoked by unit_ok.
def l24_probe():
    p=subprocess.run([str(ROOT/'tests/mergebase_unit')],cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.PIPE);assert p.returncode==0,(p.stdout+p.stderr).decode()
mark('L24',l24_probe)
def l25_26_27():
    r=mkrepo();Path(r,'x').write_text('base');save(r,'b',1);run(['branch','create','d'],r,ok=True);Path(r,'x').write_text('ours');save(r,'o',2);run(['switch','d'],r,ok=True);Path(r,'x').unlink();Path(r,'x').mkdir();Path(r,'x/y').write_text('theirs');save(r,'t',3);run(['switch','main'],r,ok=True);run(['merge','d'],r,ok=False);assert Path(r,'x').is_file() and Path(r,'x').read_text()=='ours';assert not any(Path(r).glob('x.*'));run(['resolve','x/y'],r,ok=False);o=run(['status','--exclude=**'],r,ok=True)[1];assert 'conflicted x' in o;Path(r,'x').write_text('r1');run(['resolve','x'],r,ok=True);Path(r,'x').write_text('r2');run(['resolve','x'],r,ok=True);run(['merge','--continue'],r,{'CVC_TEST_TIMESTAMP':4},ok=True);assert Path(r,'x').read_text()=='r2';shutil.rmtree(r)
mark(['L25','L26','L27'],l25_26_27)
def l28():
    r=mkrepo();Path(r,'f').write_text('base');Path(r,'g').write_text('base');save(r,'b',1);run(['branch','create','d'],r,ok=True);Path(r,'f').write_text('O');save(r,'o',2);oh=head(r);run(['switch','d'],r,ok=True);Path(r,'f').write_text('T');Path(r,'g').write_text('Tg');save(r,'t',3);run(['switch','main'],r,ok=True);run(['merge','d'],r,ok=False);Path(r,'f').write_text('R');run(['resolve','f'],r,ok=True);run(['merge','--continue'],r,{'CVC_TEST_TIMESTAMP':4,'LD_PRELOAD':FAULT,'FI_MODE':'ref-main'},ok=False);assert Path(r,'g').read_text()=='Tg';run(['merge','--abort'],r,ok=True);assert head(r)==oh and Path(r,'f').read_text()=='O' and Path(r,'g').read_text()=='base';shutil.rmtree(r)
    r,intended=finalizing_repo();refpath(r).write_text(intended+'\n');run(['merge','--abort'],r,ok=False);assert head(r)==intended and not (Path(r)/'.cvc/state/merge').exists();shutil.rmtree(r)
mark('L28',l28)

# Rollback.
def m_core():
    r=mkrepo();Path(r,'x').write_text('v1');save(r,'v1',1);v1=head(r);Path(r,'x').write_text('v2');save(r,'v2',2);v2=head(r);run(['branch','create','other'],r,ok=True);run(['switch','other'],r,ok=True);Path(r,'x').write_text('other');save(r,'other',3);other=head(r);run(['switch','main'],r,ok=True);run(['rollback',v1,'-m','rb'],r,{'CVC_TEST_TIMESTAMP':4},ok=True);rb=head(r);root,pars,_,_=commit_info(r,rb);assert pars==[v2] and snapshot(r,rb)==snapshot(r,v1) and rb not in [v1,v2];assert commit_info(r,v2)[1]==[v1];run(['rollback',other,'-m','cross'],r,{'CVC_TEST_TIMESTAMP':5},ok=True);assert snapshot(r)==snapshot(r,other);shutil.rmtree(r)
mark(['M01','M02','M03','M04','M05'],m_core)
def m06():
    r=mkrepo();Path(r,'x').write_text('a');save(r,'a',1);Path(r,'x').write_text('b');save(r,'b',2);v=head(r);Path(r,'dirty').write_text('x');run(['rollback',v,'-m','r'],r,{'CVC_TEST_TIMESTAMP':3},ok=False);assert head(r)==v;shutil.rmtree(r)
mark('M06',m06)
def m07():
    r=mkrepo();Path(r,'x').write_text('a');save(r,'a',1);v1=head(r);Path(r,'x').unlink();save(r,'del',2);v2=head(r);Path(r,'x').write_bytes(b'\0collision');run(['rollback',v1,'-m','r'],r,{'CVC_TEST_TIMESTAMP':3},ok=False);assert head(r)==v2 and Path(r,'x').read_bytes()==b'\0collision';shutil.rmtree(r)
mark('M07',m07)
def m08():
    r=mkrepo();Path(r,'x').write_text('a');save(r,'a',1);h=head(r);run(['rollback',h,'-m','same'],r,{'CVC_TEST_TIMESTAMP':2},ok=True);assert head(r)!=h and commit_info(r,head(r))[1]==[h] and snapshot(r)==snapshot(r,h);shutil.rmtree(r)
mark('M08',m08)
def m09():
    r=mkrepo();run(['rollback','main','-m','x'],r,ok=False);assert head(r)=='';shutil.rmtree(r)
mark('M09',m09)

# Verification/corruption.
def n01_10():
    r=mkrepo();Path(r,'x').write_text('x');save(r,'x',1);make_loose(r,'blob',b'unreachable');run(['verify'],r,ok=True);shutil.rmtree(r)
mark(['N01','N10'],n01_10)
def n02():
    r=mkrepo();Path(r,'x').write_text('x');save(r,'x',1);h=snapshot(r)['x'][1];objpath(r,h).write_bytes(objpath(r,h).read_bytes()+b'!');run(['verify'],r,ok=False);shutil.rmtree(r)
mark('N02',n02)
def n03():
    r=mkrepo();Path(r,'x').write_text('x');save(r,'x',1);objpath(r,snapshot(r)['x'][1]).unlink();run(['verify'],r,ok=False);shutil.rmtree(r)
mark('N03',n03)
def n04():
    r=mkrepo();Path(r,'x').write_text('x');save(r,'x',1);root=commit_info(r,head(r))[0];objpath(r,root).unlink();run(['verify'],r,ok=False);shutil.rmtree(r)
mark('N04',n04)
def n05():
    r=mkrepo();et=empty_tree(r);bad=make_loose(r,'tree',b'bad');c=make_loose(r,'commit',commit_bytes(bad,(),1,b'x'));refpath(r).write_text(c+'\n');run(['verify'],r,ok=False);shutil.rmtree(r)
mark('N05',n05)
def n06():
    r=mkrepo();bad=make_loose(r,'commit',b'bad');refpath(r).write_text(bad+'\n');run(['verify'],r,ok=False);shutil.rmtree(r)
mark('N06',n06)
def n07():
    r=mkrepo();refpath(r).write_text('not-a-ref\n');run(['verify'],r,ok=False);shutil.rmtree(r)
mark('N07',n07)
def n08():
    r=mkrepo();(Path(r)/'.cvc/HEAD').write_text('bad\n');run(['verify'],r,ok=False);shutil.rmtree(r)
mark('N08',n08)
def n09():
    r=mkrepo();raw_config(r,b'{"format_version":2}\n');run(['verify'],r,ok=False);shutil.rmtree(r)
mark('N09',n09)
def n11():
    r=mkrepo();Path(r,'x').write_text('x');save(r,'x',1);cfg=Path(r)/'.cvc/config.json';real=Path(r)/'cfg';real.write_bytes(cfg.read_bytes());cfg.unlink();os.symlink(real,cfg);run(['verify'],r,ok=False);shutil.rmtree(r)
    r=mkrepo();Path(r,'x').write_text('x');save(r,'x',1);h=snapshot(r)['x'][1];fan=objpath(r,h).parent;tmp=Path(r)/'fan';fan.rename(tmp);os.symlink(tmp,fan);run(['verify'],r,ok=False);shutil.rmtree(r)
mark('N11',n11)
def tree_entry(ty,name,oid):return bytes([ty])+struct.pack('>I',len(name))+name+bytes.fromhex(oid)
def n12():
    for typ,payload in [('blob',b'\0bad'),('symlink',b'a\0b')]:
        r=mkrepo();bad=make_loose(r,typ,payload);tree=make_loose(r,'tree',struct.pack('>I',1)+tree_entry(1 if typ=='blob' else 2,b'x',bad));c=make_loose(r,'commit',commit_bytes(tree,(),1,b'x'));refpath(r).write_text(c+'\n');run(['verify'],r,ok=False);shutil.rmtree(r)
mark('N12',n12)
def n13():
    r=mkrepo();make_loose(r,'blob',b'valid');bad='abcdef12'+'0'*56;p=objpath(r,bad);p.parent.mkdir(exist_ok=True);p.write_bytes(b'bad');run(['verify'],r,ok=False);shutil.rmtree(r)
mark('N13',n13)
def n14():
    r=mkrepo();tree=empty_tree(r);badref=make_loose(r,'tree',struct.pack('>I',1)+tree_entry(1,b'x',tree));run(['verify'],r,ok=False);shutil.rmtree(r)
mark('N14',n14)
def n15():
    r=mkrepo();missing='11'*32;make_loose(r,'tree',struct.pack('>I',1)+tree_entry(1,b'x',missing));run(['verify'],r,ok=False);shutil.rmtree(r)
mark('N15',n15)

# Locking and failure safety.
def o01_02():
    r=mkrepo();lp=Path(r)/'.cvc/lock';fd=os.open(lp,os.O_RDWR);fcntl.lockf(fd,fcntl.LOCK_EX|fcntl.LOCK_NB,0,0,0);run(['save','-m','x'],r,ok=False);run(['status'],r,ok=False);fcntl.lockf(fd,fcntl.LOCK_UN);os.close(fd)
    fd=os.open(lp,os.O_RDONLY);fcntl.lockf(fd,fcntl.LOCK_SH|fcntl.LOCK_NB,0,0,0);run(['status'],r,ok=True);fcntl.lockf(fd,fcntl.LOCK_UN);os.close(fd);shutil.rmtree(r)
mark(['O01','O02'],o01_02)
def o03():
    r=mkrepo();Path(r,'x').write_text('x');old=head(r);run(['save','-m','x'],r,{'CVC_TEST_TIMESTAMP':1,'LD_PRELOAD':FAULT,'FI_MODE':'object-link'},ok=False);assert head(r)==old;shutil.rmtree(r)
mark('O03',o03)
def o04():
    r=mkrepo();Path(r,'x').write_text('x');old=head(r);run(['save','-m','x'],r,{'CVC_TEST_TIMESTAMP':1,'LD_PRELOAD':FAULT,'FI_MODE':'ref-main'},ok=False);assert head(r)==old and 'added' in run(['status'],r,ok=True)[1];shutil.rmtree(r)
mark('O04',o04)
def o05():
    r=mkrepo();d=Path(r)/'.cvc/objects/aa';d.mkdir();(d/'.tmp-object-orphan').write_bytes(b'garbage');run(['verify'],r,ok=True);shutil.rmtree(r)
mark('O05',o05)
def o06():
    r=mkrepo();Path(r,'a.txt').write_text('A0');Path(r,'b.txt').write_text('B0');save(r,'m',1);run(['branch','create','d'],r,ok=True);run(['switch','d'],r,ok=True);Path(r,'a.txt').write_text('A1');Path(r,'b.txt').write_text('B1');save(r,'d',2);run(['switch','main'],r,ok=True);before=(Path(r,'a.txt').read_bytes(),Path(r,'b.txt').read_bytes(),head(r));run(['switch','d'],r,{'LD_PRELOAD':FAULT,'FI_MODE':'work','FI_MATCH':'b.txt'},ok=False);assert (Path(r,'a.txt').read_bytes(),Path(r,'b.txt').read_bytes(),head(r))==before;shutil.rmtree(r)
mark('O06',o06)
def o07():
    r=mkrepo();Path(r,'x').write_text('M');save(r,'m',1);run(['branch','create','d'],r,ok=True);run(['switch','d'],r,ok=True);Path(r,'x').write_text('D');save(r,'d',2);run(['switch','main'],r,ok=True);before=Path(r,'x').read_bytes();run(['switch','d'],r,{'LD_PRELOAD':FAULT,'FI_MODE':'head'},ok=False);assert Path(r,'x').read_bytes()==before and (Path(r)/'.cvc/HEAD').read_text()=='ref: refs/heads/main\n';shutil.rmtree(r)
mark('O07',o07)
def o08():
    r=mkrepo();Path(r,'x').write_text('x');save(r,'x',1);lp=Path(r)/'.cvc/lock';fd=os.open(lp,os.O_RDWR);fcntl.lockf(fd,fcntl.LOCK_EX|fcntl.LOCK_NB,0,0,0);run(['restore','x','--from','main'],r,ok=False);fcntl.lockf(fd,fcntl.LOCK_UN);os.close(fd);shutil.rmtree(r)
mark('O08',o08)
def o09():
    r=mkrepo();Path(r,'x').write_text('M');save(r,'m',1);run(['branch','create','d'],r,ok=True);run(['switch','d'],r,ok=True);Path(r,'x').write_text('D');save(r,'d',2);run(['switch','main'],r,ok=True);outside_dir=tempfile.mkdtemp(prefix='cvc-hardlink-outside-');outside=Path(outside_dir,'outside');os.link(Path(r,'x'),outside);ino=outside.stat().st_ino;run(['switch','d'],r,ok=True);assert outside.read_text()=='M' and Path(r,'x').read_text()=='D' and Path(r,'x').stat().st_ino!=ino;shutil.rmtree(outside_dir);shutil.rmtree(r)
mark('O09',o09)
def o10():
    r=mkrepo();lp=Path(r)/'.cvc/lock';fd=os.open(lp,os.O_RDWR);fcntl.lockf(fd,fcntl.LOCK_EX|fcntl.LOCK_NB,1,100,os.SEEK_SET);run(['status'],r,ok=False);fcntl.lockf(fd,fcntl.LOCK_UN,1,100,os.SEEK_SET);os.close(fd);assert lp.stat().st_size==0;shutil.rmtree(r)
mark('O10',o10)

EXPECTED=[]
for letter,maxn in [('A',9),('B',7),('C',20),('D',14),('E',15),('F',8),('G',11),('H',14),('I',13),('J',14),('K',13),('L',28),('M',9),('N',15),('O',10)]:
    EXPECTED += [f'{letter}{i:02d}' for i in range(1,maxn+1)]
EXPECTED += ['J11b','L07b']
# J numbering has J01..J14 plus J11b; L has L01..L28 plus L07b.
missing=[x for x in EXPECTED if x not in RESULT]
extra=[x for x in RESULT if x not in EXPECTED]
for x in EXPECTED:
    if x in RESULT: print(f'{x}: {RESULT[x]} - {DETAIL[x]}')
if missing:print('MISSING:',','.join(missing))
if extra:print('EXTRA:',','.join(extra))
passed=sum(RESULT.get(x)=='PASS' for x in EXPECTED);failed=sum(RESULT.get(x)=='FAIL' for x in EXPECTED)
print(f'ACCEPTANCE SUMMARY: {passed}/{len(EXPECTED)} PASS, {failed} FAIL, {len(missing)} MISSING, 0 SKIPPED')
if failed or missing or extra:sys.exit(1)
