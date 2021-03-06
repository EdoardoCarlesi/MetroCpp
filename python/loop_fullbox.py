#!/opt/rh/rh-python36/root/usr/bin/python3
###########/usr/bin/python3

from mtreelib.sqllib import *
from mtreelib.mtree import *
from mtreelib.read_mtree import *
import timeit
import os
import warnings
warnings.simplefilter(action='ignore', category=FutureWarning)

''' 
	loop_treedir.py
	Extract trees and store them into a database
'''

print('Testing Merger Tree python post-processing scripts')

#filePrefix='/fullbox_'
filePrefix='/fb_'

#baseTreeMCPP = '/home/edoardo/CLUES/MetroC++/output/FullBox/'
baseTreeMCPP = '/home/eduardo/CLUES/DATA/FullBox/trees/'
#baseTreeMCPP = '/z/carlesi/CLUES/DATA/trees/FullBox/'
suffTreeMCPP = 'mtree'

nSnaps = 54	
nSteps = 54
nChunk = 1
#nChunk = 24

iSeedIni = 0
iSeedEnd = 2

iTree = 0

# Loop on the large scale seed codes
for iSeed in range(iSeedIni, iSeedEnd):

	# Save all the extracted trees into a database
	thisDb = baseTreeMCPP + 'fullbox_' + '%02d' % iSeed + '_trees.db'

	#Initialize the database and begin transaction. This avoids to commit at every step and speeds up the program
	newSql = SQL_IO(thisDb, nSteps)
	newSql.halo_table()
	newSql.cursor.execute('BEGIN TRANSACTION')

	iSeedStr = '%02d' % iSeed
	thisSubDir = iSeedStr 
	thisTreePath = baseTreeMCPP + thisSubDir
	#rootFile = filePrefix + thisSubDir + '_'	
	#rootFile = filePrefix + thisSubDir + '_nobuff_'	
	#rootFile = filePrefix + '00_nobuff_'	
	rootFile = filePrefix + thisSubDir + '_gather_'	
	testFile = rootFile + '%03d' % nSnaps + '.0.' + suffTreeMCPP
	
	print('Checking if folder %s exist. ' % thisTreePath )
	print('Checking if file %s exists.' % (thisTreePath + testFile))

	# If this path exists then extract the merger tree therein
	if os.path.isdir(thisTreePath) and os.path.isfile(thisTreePath+testFile):
		readFiles = ReadSettings(thisTreePath+rootFile, suffTreeMCPP, nChunk, nSnaps, nSteps)
		allTrees = readFiles.read_trees()
		
		start = timeit.default_timer()
		print('Adding %s to database %s.' % (thisSubDir, thisDb))

		# Store all the trees inside a database
		for thisTree in allTrees:
			[tmp_m, tmp_id] = thisTree.get_mass_id()

			if tmp_m[0] > 500:
				newSql.insert_tree(tmp_id[0], thisSubDir, tmp_m, tmp_id)
				iTree += 1

		end = timeit.default_timer()
		print('Inserted %d trees in %f seconds.' % (iTree, end - start))

	try: 
		# Now commit to the database and close
		newSql.cursor.execute('COMMIT')
		newSql.close()
	except:
		'Do nothing'
