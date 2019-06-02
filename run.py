# ./qemu-system-aarch64 | ./qemu-system-i386
# -m 4096M 
# -M virt 
# -cpu cortex-a57 
# -global virtio-blk-device.scsi=off 
# -device virtio-scsi-device,id=scsi 
# -drive file=arm64disk.img,id=coreimg,cache=unsafe,if=none 
# -device scsi-hd,drive=coreimg 
# -netdev user,id=unet 
# -device virtio-net-device,netdev=unet 
# -kernel vmlinuz 
# -initrd initrd.img 
# -append root=/dev/sda2 
# -nographic 
# --plugin file=../../plugins/exec-log/exec-log.so
# https://manpages.debian.org/stretch/qemu-system-x86/qemu-system-x86_64.1.en.html
import argparse
import json
import pprint
import os.path
import socket
arm64 = {'cpu': 'cortex-a57', 'arch': 'aarch64', 'path': 'buildDirARM64/aarch64-softmmu/'}
x86 = {'cpu': '', 'arch': 'i386', 'path': 'buildDirX86/i386-softmmu/'}
arcTypes = {'arm64': arm64, 'i386': x86 }
arch = arm64
#config.json keep arch dependent params out of here
configPath = ''
imgConfigPath = ''
pickedArch = ''
benchmarkPath = ''
pluginPath = ''
def choose(args):
    global configPath
    configPath = args.config
    global pickedArch
    pickedArch = args.arch
    global imgConfigPath
    imgConfigPath = args.imgconfig
    global benchmarkPath
    benchmarkPath = args.benchmark
    global pluginPath
    pluginPath = args.plugin

parser = argparse.ArgumentParser()
parser.add_argument('--arch', default="arm64")
parser.add_argument('--config', default="config.json")
parser.add_argument('--imgconfig',default='')
parser.add_argument('--benchmark',default='')
parser.add_argument('--plugin',default='')
parser.set_defaults(func=choose)

def main():
    if(pickedArch != 'arm64' and pickedArch != 'i386' ):
        print("only supported achitectures are: ")
        pprint.pprint(arcTypes.keys())
        return
    global arch 
    arch = arcTypes[pickedArch]
    configJson = None
    imgConfigJson = None
    if(configPath != ''):
        configFile = open(configPath, "r")
        configJson = json.loads(configFile.read())
    else:
        print("need a config file to run")
        return
    
    if(imgConfigPath != ''):
        imgConfigFile = open(imgConfigPath, "r")
        imgConfigJson = json.loads(imgConfigFile.read())
    else:
        print("need images to run")
        return
    
    if(benchmarkPath == '' or  not os.path.isdir(benchmarkPath)):
        print "path to benchmark is missing or not a directory"
        return
    if(pluginPath == '' or not os.path.isfile(pluginPath)):
        print "path to plugin.so is missing or not a file"
        return

    execStr = "./{}qemu-system-{} -cpu {} ".format(arch['path'], arch['arch'],arch['cpu'])
    
    memory = configJson.get('memory')
    if(memory != None) :
        execStr = execStr + " -m {} ".format(memory)
    
    emuMachineType = configJson.get('emuMachineType')
    if(emuMachineType != None):
        execStr = execStr + " -M {} ".format(emuMachineType)
    
    globalFlag = configJson.get('global')
    if(globalFlag != None):
        execStr = execStr + " -global {} ".format(globalFlag)
    
    deviceArr = configJson.get('devices')
    if(deviceArr != None):
        for device in deviceArr:
            execStr = execStr + " -device {} ".format(device)

    netdev = configJson.get('netdev')
    portNumber = configJson.get('portNumber')
    if(portNumber is None):
        portNumber = 2222
    else:
        try:
            portNumber = int(portNumber)
        except ValueError:
            print("port number isn not an int and can not be converted to one!")
            return
    
    remoteBenchmarkDir = configJson.get('remoteBenchmarkDir')
    if (remoteBenchmarkDir is None) :
        remoteBenchmarkDir = ""

    remotescriptExec = configJson.get('remotescriptExec')
    if (remotescriptExec is None) :
        remotescriptExec = "runner.sh"

    if(netdev != None):
        execStr = execStr + " -netdev {},hostfwd=tcp::{}-:22".format(netdev,portNumber)
    
    append = configJson.get('append')
    if(append != None):
        execStr = execStr + " -append {} ".format(append)

    nographic = configJson.get('nographic')
    if(nographic != None):
        execStr = execStr + " -nographic"
    
    imgDir = os.path.dirname(imgConfigPath)
    kernel = imgConfigJson.get('kernel')
    if(kernel != None):
        execStr = execStr + " -kernel {}/{} ".format(imgDir, kernel)
    
    initrd = imgConfigJson.get('initrd')
    if(initrd != None):
        execStr = execStr + " -initrd {}/{} ".format(imgDir, initrd)
    
    drive = imgConfigJson.get('drive')
    driveParam = configJson.get('driveParam')
    if(drive != None and driveParam != None):
        execStr = execStr + " -drive file={}/{},{}".format(imgDir, drive,driveParam)
    
    execStr = execStr + " --plugin file={}".format(pluginPath)
    print(execStr)
    os.system(execStr+"&")
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    while( sock.connect_ex(('0.0.0.0',portNumber)) !=0):
        continue
    print("vm is booted")
    sock.close()
    password = imgConfigJson.get('password')
    username = imgConfigJson.get('username')
    # ssh qsim@0.0.0.0 -p 2222
    scpExec = "sshpass -p  {} scp -P {} -r {} {}@0.0.0.0:{}".format(password, portNumber, benchmarkPath, username, remoteBenchmarkDir)
    print(scpExec)
    os.system(scpExec)
    #ssh qsim@0.0.0.0 -p 2222
    stripBmPath = benchmarkPath
    if(benchmarkPath[-1] == '/'):
        stripBmPath = stripBmPath[:-1]
    stripBmPath = os.path.basename(stripBmPath)
    sshExec = "sshpass -p  {} ssh {}@0.0.0.0 -p {} 'cd {}/{} ; ./{}' ".format(password, username,portNumber,remoteBenchmarkDir,stripBmPath,remotescriptExec)
    print(sshExec)
    os.system(sshExec)

if __name__ == "__main__":
    args = parser.parse_args()
    args.func(args)
    main()
