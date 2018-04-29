
# Python3 compatible

import os
import subprocess

rootdir = os.path.abspath(os.path.join(__file__, "..", ".."))

multiprocess_reference_py = os.path.join(rootdir, "tools", "multiprocess_reference.py")

TASK_LIST = [

]

for task in TASK_LIST:

    taskdir = os.path.dirname(task)
    print("Changing directory to {}".format(taskdir))
    os.chdir(taskdir)

    cmd = []
    cmd.append("python3")
    cmd.append(multiprocess_reference_py)
    cmd.append(task)
    cmd.append("--reference=20")

    print("Executing cmd {}".format(cmd))
    subprocess.call(cmd)
