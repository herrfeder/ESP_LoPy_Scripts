#!/usr/bin/python

from optparse import OptionParser
import pexpect
import logging
import sys
import pdb
import re

def login(host):

    child = pexpect.spawn("ftp -p "+host)
    child.expect('Name')
    child.sendline("micro")
    child.expect(r'Password:')
    child.sendline("python")

    status = child.expect([pexpect.TIMEOUT,r'Login failed.',r'Using binary mode to transfer files.'],timeout=3)
    if status == 1:
        logging.warning("Login failed")
        sys.exit()
    if status == 2:
        logging.warning("Login successful")
        return child

def upload(child,up_file,new_file,host):
    child.sendline("cd flash")
    status = child.expect([pexpect.TIMEOUT,r'221',r'250'],timeout=3)
    if status == 1:
        logging.warning("Session died, new login")
        child = login(host)
    if status == 2:
        child.sendline("put "+up_file+" "+new_file)
        status = child.expect([pexpect.TIMEOUT,'221','bytes sent'],timeout=3)
        if status == 1:
            logging.warning("Session died, exiting")
            sys.exit()
        if status == 2:
            logging.warning("Uploaded file "+up_file+" as "+new_file+" successful")
            sys.exit()



def remove(child,basestr,host):
    filelist = []
    child.sendline("cd flash")
    status = child.expect([pexpect.TIMEOUT,r'221',r'250'],timeout=3)
    if status == 1:
        logging.warning("Session died, new login")
        child = login(host)
    if status == 2:
        print("send dir")
        child.sendline("dir")
        status = child.expect([pexpect.TIMEOUT,'226'])
        if status == 1:
            pbuffer = child.before
            lines = pbuffer.split("   ")
            for line in lines:
                mat = re.search(".py",line)
                if mat:
                    filelist.append(mat.string.rsplit(' ',1)[1].split('\r\n')[0])
            for fileitem in filelist:
                mat = re.search(basestr,fileitem)
                if mat:
                    child.sendline("del "+fileitem)
                    status = child.expect([pexpect.TIMEOUT,'250'])
                    if status == 1:
                        logging.warning("Deleted "+fileitem)


if __name__=="__main__":

    parser = OptionParser()
    parser.add_option("-H", "--host", action="store", type="string",dest="host")
    parser.add_option("-f", "--file", action="store", type="string",dest="up_file")
    parser.add_option("-n", "--new", action="store", type="string",dest="new_file")
    parser.add_option("-r", "--remfile", action="store", type="string", dest="remfile")
    (options, args) = parser.parse_args()

    host = options.host
    up_file = options.up_file
    new_file = options.new_file
    remfile = options.remfile

    child = login(host)

    if up_file:
        status = upload(child,up_file,new_file,host)
    if remfile:
        status = remove(child,remfile,host)
