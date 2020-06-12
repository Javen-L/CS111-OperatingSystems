# Written by Adam Young youncadam@ucla.edu
import sys
import csv


# global data structures used throughout the script
# use dicts for block, inode, parent inode, and directory entry information. 
blocks = {} 
dirs = {} 
inodes = {} # this will be especially useful for reference counts
icounts = {} # dict for link counts for each inode
pinodes = {} # dict for parent inodes

nblocks = 0
ninodes = 0
bsize = 0
isize = 0

#free lists for inodes and blocks
blockfl = set() 
inodefl = set()

# function that parses csv and populates global data structures for easier functions later, 
# and outputs requested Block Consistency Audits
def filesystemaudit(fd,datafile):
	corrupt = False
	fd.seek(0,0)
	#parse csv line by line (sections refers to the array of columns in each line)
	for sections in datafile:		
		if sections[0] == "SUPERBLOCK":
			nblocks = int(sections[1])
			ninodes = int(sections[2])
			bsize = int(sections[3])
			isize = int(sections[4])

		elif sections[0] == "GROUP":
			nblocks = int(sections[2])
			ninodes = int(sections[3])
			groupblock = (isize*ninodes)/bsize + int(sections[8])

		elif sections[0] == "BFREE":
			blockfl.add(int(sections[1]))

		elif sections[0] == "IFREE":
			inodefl.add(int(sections[1]))

		elif sections[0] == 'INODE':
			icounts[int(sections[1])] = int(sections[6]) #record # of link counts for each inode 
			ninode = int(sections[1]) #record inode number			
			for i in range(12, 27):
				bnum = int(sections[i])
				if bnum == 0: #if you don't skip 0th block, you get a bunch of duplicate references
					continue
				if i == 24:
					offset = 12
					inodetype = "INDIRECT "
				elif i == 25:
					inodetype = "DOUBLE INDIRECT "
					offset = 268
				elif i == 26:
					inodetype = "TRIPLE INDIRECT "
					offset = 65804
				else:
					inodetype = ""
					offset = 0

				# check for invalid/reserved blocks
				if bnum < 0 or bnum > nblocks:
					print("INVALID %sBLOCK %d IN INODE %d AT OFFSET %d" %(inodetype, bnum, ninode, offset))
					corrupt = True

				# check if block is reserved
				elif (bnum > 0 and bnum <= 7) or bnum == 64: 
					print("RESERVED %sBLOCK %d IN INODE %d AT OFFSET %d" %(inodetype, bnum, ninode, offset))
					corrupt = True
				elif bnum not in blocks:
					temp = [ninode, inodetype, offset]
					blocks[bnum] = [temp]
				
				#append additional references to the same block
				else:
					temp = [ninode, inodetype, offset]
					blocks[bnum].append(temp)

		elif sections[0] == "INDIRECT":
			depth = int(sections[2])
			bnum = int(sections[5])
			inum = sections[1]
			if depth == 1:
				inodetype = "INDIRECT "
				offset = 12
			elif depth == 2:
				inodetype = "DOUBLE INDIRECT "
				offset = 268
			elif depth == 3:
				inodetype = "TRIPLE INDIRECT "
				offset = 65804
			
			# now check for invalid/reserved the same way as last time
			temp = [ninode, inodetype, offset]
			if bnum < 0 or bnum > nblocks:				 
				print("INVALID %sBLOCK %d IN INODE %d AT OFFSET %d" %(inodetype, bnum, ninode, offset))
				corrupt = True	
			elif (bnum >= 0 and bnum <= 7) or bnum == 64:
				print("RESERVED %sBLOCK %d IN INODE %d AT OFFSET %d" %(inodetype, bnum, ninode, offset))
				corrupt = True
			elif bnum not in blocks:
				blocks[bnum] = [temp]
			else:
				blocks[bnum].append(temp)

		elif sections[0] == "DIRENT":
			pinode = int(sections[1])
			ninode = int(sections[3])
			name = sections[6]
			dirs[ninode] = sections[6]

			#check for previous references to this inode
			if ninode in inodes:
				inodes[ninode] += 1
			else:
				inodes[ninode] = 1

			#check for corrupted inodes
			if ninode < 1 or ninode > ninodes:
				print("DIRECTORY INODE %d NAME %s INVALID INODE %d" %(pinode, name, ninode))
				corrupt = True

			if name == "'..'" and pinode in pinodes and ninode != pinodes[pinode]:
				corrupt = True
				print("DIRECTORY INODE %d NAME %s LINK TO INODE %d SHOULD BE %d" %(pinode, name, ninode, pinode))

			if name == "'..'":
				pinodes[ninode] = pinode

			if pinode != ninode and name == "'.'":
				print("DIRECTORY INODE %d NAME %s INVALID INODE %d" %(pinode, name, ninode, pinode))
				corrupt = True

			elif name == "'.'":
				continue

			else:
				pinodes[ninode] = pinode


	# check for duplicate blocks
	for temp in blocks:
		if len(blocks[temp]) > 1: # if there are multiple references to the same block			
			for i in blocks[temp]:
				if i[1] == "":
					print("DUPLICATE BLOCK %s IN INODE %s AT OFFSET %s" %(temp, i[0], i[2]))
				else:
					print("DUPLICATE %sBLOCK %s IN INODE %s AT OFFSET %s" %(i[1], temp, i[0], i[2]))

	# check for corrupted blocks by scanning allocated blocks, blocks on free list, and reserved blocks
	for i in range(1, nblocks + 1):		
		if i not in blocks and i not in blockfl and i not in (0, 1, 2, 3, 4, 5, 6, 7, 64):
			print("UNREFERENCED BLOCK %d" %(i))
			corrupt = True
		elif i in blockfl and i in blocks:
			print("ALLOCATED BLOCK %d ON FREELIST" %(i))
			corrupt = True

	# check for corrupted inodes by checking free inode list, link count list for each inode, and allocated inodes
	for i in range(1, ninodes + 1): #
		if i not in inodefl and i not in inodes and i not in icounts and i not in (1, 3, 4, 5, 6, 7, 8, 9, 10):
			print("UNALLOCATED INODE %s NOT ON FREELIST" %(i))
			corrupt = True
		if i in icounts and i in inodefl:
			print("ALLOCATED INODE %d ON FREELIST" %(i))
			corrupt = True
			inodefl.remove(i)

	# ensure inode linkcount is equal to number of links for that inode
	for i in icounts:
		icount = icounts[i]
		if i not in inodes:
			temp = 0
		else:
			temp = inodes[i]

		if temp != icount:
			corrupt = True
			print("INODE %d HAS %d LINKS BUT LINKCOUNT IS %d" %(i, temp, icount))


	# check for directories containing unallocated inodes
	for i in dirs:
		if i in inodefl and i in pinodes:
			corrupt = True
			print("DIRECTORY INODE %d NAME %s UNALLOCATED INODE %d" %(pinodes[i], dirs[i], i))

	# return whether or not the analyzed filesystem is corrupted
	return corrupt


def main():

	if(len(sys.argv) != 2):
		sys.stderr.write("Invalid arguments.\n")
		sys.exit(1)

	try:
		fd = open(sys.argv[1], "r")
	
	except IOError:
		sys.stderr.write("Error: could not open input file. Exiting...")
		exit(1)
		
	datafile = csv.reader(fd)

	exitcode = 0
	if(filesystemaudit(fd, datafile)):
		exitcode = 2

	exit(exitcode)


if __name__ == '__main__':
	main()