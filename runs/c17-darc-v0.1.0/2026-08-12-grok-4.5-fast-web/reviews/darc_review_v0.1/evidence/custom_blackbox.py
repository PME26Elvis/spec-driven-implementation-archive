import os, subprocess, tempfile, json, pathlib, shutil, stat, time, hashlib, re
D='/mnt/data/darc_review_work/darc/bin/darc'
results=[]
def run(args, cwd=None):
    p=subprocess.run([D]+args,cwd=cwd,text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    return p.returncode,p.stdout,p.stderr
def case(name, ok, detail):
    results.append((name,ok,detail)); print(('PASS' if ok else 'FAIL'),name,'-',detail)

base=tempfile.mkdtemp(prefix='darc_independent_')
try:
    # duplicate independent files -> should remain independent
    src=os.path.join(base,'src'); repo=os.path.join(base,'repo'); out=os.path.join(base,'out')
    os.mkdir(src); open(os.path.join(src,'a'),'wb').write(b'same-data'); shutil.copyfile(os.path.join(src,'a'),os.path.join(src,'b'))
    run(['init',repo]); run(['--repo',repo,'snapshot','create',src,'--timestamp','1'])
    rc,so,se=run(['--repo',repo,'snapshot','list']); sid=so.splitlines()[1].split()[0]
    run(['--repo',repo,'restore',sid,'--to',out])
    a=os.path.join(out,'src','a'); b=os.path.join(out,'src','b')
    independent=os.stat(a).st_ino != os.stat(b).st_ino
    case('duplicate-independent-restore', independent, f'inodes {os.stat(a).st_ino},{os.stat(b).st_ino}; spec requires independent files')

    # metadata only diff chmod
    os.chmod(os.path.join(src,'a'),0o600); run(['--repo',repo,'snapshot','create',src,'--timestamp','2'])
    rc,so,se=run(['--repo',repo,'snapshot','list']); ids=[l.split()[0] for l in so.splitlines()[1:]]
    rc,dout,de=run(['--repo',repo,'snapshot','diff',ids[1],ids[0],'--format','json'])
    try: dj=json.loads(dout); changed=sum(dj.get(k,0) for k in ['added','removed','modified'])
    except: changed=-1
    case('diff-metadata-only', changed>0 or 'metadata' in dout.lower(), f'output={dout.strip()}')

    # partial restore should only selected
    out2=os.path.join(base,'out2')
    rc,so,se=run(['--repo',repo,'restore',ids[0],'--to',out2,'--path','src/a'])
    entries=[]
    for root,ds,fs in os.walk(out2): entries += [os.path.relpath(os.path.join(root,f),out2) for f in fs]
    case('partial-restore-filter', entries==['src/a'], f'restored files={entries}')

    # overwrite never must conflict, not silently skip
    out3=os.path.join(base,'out3'); os.makedirs(os.path.join(out3,'src')); open(os.path.join(out3,'src','a'),'wb').write(b'SENTINEL')
    rc,so,se=run(['--repo',repo,'restore',ids[0],'--to',out3,'--overwrite','never'])
    content=open(os.path.join(out3,'src','a'),'rb').read()
    case('overwrite-never-conflict', rc==8, f'rc={rc}, existing={content!r}, stdout={so.strip()}, stderr={se.strip()}')

    # default parent should be HEAD when omitted
    src2=os.path.join(base,'parentsrc'); repo2=os.path.join(base,'parentrepo'); os.mkdir(src2); open(os.path.join(src2,'x'),'w').write('1')
    run(['init',repo2]); run(['--repo',repo2,'snapshot','create',src2,'--timestamp','1']); open(os.path.join(src2,'x'),'w').write('2'); run(['--repo',repo2,'snapshot','create',src2,'--timestamp','2'])
    rc,ls,_=run(['--repo',repo2,'snapshot','list']); newest=ls.splitlines()[1].split()[0]
    rc,show,_=run(['--repo',repo2,'snapshot','show',newest])
    case('default-parent-head', 'Parent:' in show and '-' not in show.split('Parent:',1)[1].splitlines()[0], f'show={show.strip()}')

    # fixed timestamp zero must remain zero; implementation treats zero as absent
    repo3=os.path.join(base,'r3'); run(['init',repo3]); run(['--repo',repo3,'snapshot','create',src2,'--timestamp','0']); rc,ls,_=run(['--repo',repo3,'snapshot','list']);
    case('timestamp-zero-honored', '1970-01-01T00:00:00Z' in ls, f'list row={ls.splitlines()[1] if len(ls.splitlines())>1 else ls}')

    # unknown config key should reject
    badcfg=os.path.join(base,'bad.json'); open(badcfg,'w').write('{"chunking":{"avg_bytes":65536,"typo_key":1}}')
    rc,so,se=run(['config','validate',badcfg]); case('config-unknown-key-reject', rc==2, f'rc={rc}, out={so.strip()}, err={se.strip()}')
    # duplicate json key should reject
    dupcfg=os.path.join(base,'dup.json'); open(dupcfg,'w').write('{"chunk_min":16384,"chunk_min":32768}')
    rc,so,se=run(['config','validate',dupcfg]); case('config-duplicate-key-reject', rc==2, f'rc={rc}, out={so.strip()}')

    # include/exclude should apply: exclude all *.tmp
    cfg=os.path.join(base,'exc.json'); open(cfg,'w').write('{"scan":{"exclude":["**/*.tmp"]}}')
    sx=os.path.join(base,'sx'); rx=os.path.join(base,'rx'); ox=os.path.join(base,'ox'); os.mkdir(sx); open(os.path.join(sx,'keep.txt'),'w').write('k'); open(os.path.join(sx,'drop.tmp'),'w').write('d')
    run(['init',rx]); run(['--repo',rx,'--config',cfg,'snapshot','create',sx,'--timestamp','1']); rc,ls,_=run(['--repo',rx,'snapshot','list']); sid=ls.splitlines()[1].split()[0]; run(['--repo',rx,'restore',sid,'--to',ox])
    case('config-exclude-applied', not os.path.exists(os.path.join(ox,'sx','drop.tmp')), f'drop.tmp exists={os.path.exists(os.path.join(ox,"sx","drop.tmp"))}')

    # special FIFO default error
    sf=os.path.join(base,'special'); rr=os.path.join(base,'specialrepo'); os.mkdir(sf); os.mkfifo(os.path.join(sf,'pipe')); run(['init',rr]); rc,so,se=run(['--repo',rr,'snapshot','create',sf,'--timestamp','1']); case('special-file-default-error', rc!=0, f'rc={rc}, out={so.strip()}, err={se.strip()}')

    # symlink target change diff
    sy=os.path.join(base,'sy'); sr=os.path.join(base,'syrepo'); os.mkdir(sy); os.symlink('a',os.path.join(sy,'l')); run(['init',sr]); run(['--repo',sr,'snapshot','create',sy,'--timestamp','1']); os.unlink(os.path.join(sy,'l')); os.symlink('b',os.path.join(sy,'l')); run(['--repo',sr,'snapshot','create',sy,'--timestamp','2']); rc,ls,_=run(['--repo',sr,'snapshot','list']); si=[l.split()[0] for l in ls.splitlines()[1:]]; rc,do,_=run(['--repo',sr,'snapshot','diff',si[1],si[0],'--format','json']);
    try: j=json.loads(do); n=j.get('modified',0)+j.get('added',0)+j.get('removed',0)
    except:n=-1
    case('diff-symlink-target', n>0, f'output={do.strip()}')

finally:
    pass

print('\nSUMMARY',sum(x[1] for x in results),'passed',sum(not x[1] for x in results),'failed','of',len(results))
open('/mnt/data/darc_review_work/custom_blackbox_results.txt','w').write('\n'.join(f'{"PASS" if o else "FAIL"} {n}: {d}' for n,o,d in results)+f'\nSUMMARY {sum(x[1] for x in results)} passed {sum(not x[1] for x in results)} failed of {len(results)}\n')
