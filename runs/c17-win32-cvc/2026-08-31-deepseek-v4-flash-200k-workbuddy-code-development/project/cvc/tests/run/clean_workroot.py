# Force-delete the accumulated test workroot reliably using shutil.
import os, shutil
target = r"D:\cvctest_auto"
if os.path.exists(target):
    for name in list(os.listdir(target)):
        p = os.path.join(target, name)
        try:
            if os.path.isdir(p) and not os.path.islink(p):
                shutil.rmtree(p)
            else:
                os.unlink(p)
        except Exception as e:
            print("ERR %s: %r" % (p, e))
    # remove the root dir itself
    try:
        os.rmdir(target)
        print("root removed")
    except Exception as e:
        print("root rmdir err: %r" % e)
else:
    print("no such dir")
print("done")
