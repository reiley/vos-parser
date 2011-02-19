#! /usr/bin/python

def escape(string):
	string=string.replace('\\','\\\\')
	string=string.replace('"','\\"')
	return ' "'+string+'"'

def lst2inl(filename,default=''):
	s=tuple(map(lambda x: x.strip().split('\t'),open(filename+'.lst').readlines()))
	s=tuple(map(lambda x: (int(x[0]), escape(x[1])),s))
	s=sorted(s)
	format='  /* %%%dd */'%(len(str(s[-1][0])))
	count=0
	stack=[]
	for i in s:
		while count<i[0]:
			stack.append(format%(count)+' '+escape(default))
			count+=1
		stack.append(format%(i[0])+' '+i[1])
		count=i[0]+1
	return 'char* '+filename+'[]={\n'+',\n'.join(stack)+'\n};\n\n'

if __name__ == "__main__":
	info=open('dump.inl','w')
	info.write(lst2inl('smf_control','Controller'))
	info.write(lst2inl('smf_instrument'))
	info.write(lst2inl('smf_meta'))
	info.write(lst2inl('vos_genre'))
	info.close()
