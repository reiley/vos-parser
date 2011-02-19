#! python

import os
import pprint
import subprocess
import sys
import time

result={
'SKIP':[],
'FAIL':[],
'PASS':[],
}
def analyse(ff):
	if (ff[-4:].lower() in ('.mid','.vos','.rmi','.kar',)) or (ff[-5:].lower() in ('.midi',)):
		proc = subprocess.Popen(['main.exe', ff])
		proc.wait()
		if proc.returncode:
			result['FAIL'].append(ff)
			return 'FAIL'
	else:
		result['SKIP'].append(ff)
		return 'SKIP'
	result['PASS'].append(ff)
	return 'PASS'
def track(curdir=os.curdir,ident=0):
	for f in os.listdir(curdir):
		ff=os.path.join(curdir,f)
		if os.path.isfile(ff):
			print(analyse(ff)+'\t'+curdir+'\\'+f)
		else:
			track(ff,ident+1)

if __name__ == "__main__":
	starttime=time.time()
	track(len(sys.argv)>1 and sys.argv[1] or '..\\..')
	finishtime=time.time()
	pprint.pprint(result)
	print(', '.join(map(lambda x: x+' %d'%len(result[x]), result)))
	print('Elapsed time %f seconds'%(finishtime-starttime))
