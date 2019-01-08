/*
 *   METROC++: MErger TRees On C++, a scalable code for the computation of merger trees in cosmological simulations.
 *   Copyright (C) Edoardo Carlesi 2018-2019
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


/*
 * MergerTree.cpp:
 * This file holds the core routines and defines the classes that are required to consistently compute 
 * the merger trees.
 */

#include <string>
#include <vector>
#include <algorithm>
#include <math.h>
#include <map>

#include "MergerTree.h"
#include "Halo.h"

#include "utils.h"
#include "global_vars.h"

using namespace std;



HaloTree::HaloTree()
{
};



HaloTree::~HaloTree()
{
	Clean();
};



void HaloTree::Clean()
{
	for (int iM = 0; iM < mTree.size(); iM++)
		mTree[iM].Clean();

	mTree.clear();
	mainHalo.clear();
};



void MergerTree::Info()
{
	cout << "Task=" << locTask << " " << mainHalo.ID << " " << idProgenitor.size() << endl;

	if (isOrphan)
		for (int iP = 0; iP < idProgenitor.size(); iP++)
			cout << "Ind=" << indexProgenitor[iP] << " ID= " << idProgenitor[iP] << endl;
	else
		for (int iP = 0; iP < idProgenitor.size(); iP++)
			cout << "Ind=" << indexProgenitor[iP] << " ID= " << idProgenitor[iP] << " NP=" << nCommon[1][iP] << endl;

};



void MergerTree::Clean()
{
	for (int iC = 0; iC < nCommon.size(); iC++)
	{
		nCommon[iC].clear();
	}

	nCommon.clear();

	idProgenitor.clear();
	indexProgenitor.clear();
	progHalos.clear();
};



MergerTree::MergerTree()
{
};

MergerTree::~MergerTree()
{
};


#ifdef CMP_MAP
void MergerTree::AssignMap()
{
	int nProgs = 0, nCommTot = 0, iP = 0;
	map<unsigned long long int, vector<int>>::iterator thisMap;
	
	nCommon.resize(nPTypes);
	
	/* Each merger tree stores the halo ids in a map that stores the number of particles shared with each progenitor */
	for (thisMap = indexCommon.begin(); thisMap != indexCommon.end(); thisMap++)
	{
		unsigned long long int thisHaloID = thisMap->first;
		vector<int> thisCommon = thisMap->second;

		nCommTot = 0;

		for (int iT = 0; iT < nPTypes; iT++)
			nCommTot += thisCommon[iT];

		if (nCommTot > minPartCmp)	
		{
			for (int iT = 0; iT < nPTypes; iT++)
				nCommon[iT].push_back(thisCommon[iT]);
			
			idProgenitor.push_back(thisHaloID);
			nProgs++;
		}

		iP++;
	}

	if (nProgs == 0)
	{
		isOrphan = true;
		
		//if (mainHalo.nPart[1] > 300)
		//	mainHalo.Info();
	} else {
		/* We allocate these vectors here, the indexes and the halos will be copied inside later on */
		isOrphan = false;
		indexProgenitor.resize(nProgs);
		progHalos.resize(nProgs);
	}
};
#endif


/* This sorting algorithm might be very inefficient but it's straightforward to implement, 
 * plus we will rarely deal with halos with more than 10^3 progenitors */
void MergerTree::SortByMerit()
{
	vector<unsigned long long int> tmpIdx;
	vector<vector<int>> tmpNCommon;
	vector<float> allMerit;
	vector<int> idx, tmpIndex;
	vector<Halo> tmpProgHalos;
	float merit = 0.0;

	//if (mainHalo.nPart[1] > 100000 && locTask == 0)
	//	cout << "Size ID Prog: " << idProgenitor.size() << endl;

	for (int iM = 0; iM < idProgenitor.size(); iM++)
	{
		int nComm = 0;
		double ratioM = 0;

		//for(int iC = 0; iC < nPTypes; iC++)
		//		nComm += nCommon[iC][iM];

		ratioM = (float) mainHalo.nPart[1] / (float) progHalos[iM].nPart[1];
	
		if (ratioM < 1.0) ratioM = 1.0 / ratioM;

		// FIXME merit is based on DM particles only!!!
		merit = nCommon[1][iM] / (ratioM*1.0001 - 1.0);
		//merit = ((float) nComm) / (mainHalo.nPart[1] * progHalos[iM].nPart[1]);
		//merit = ((float) nCommon[1][iM]) / (mainHalo.nPart[1] * progHalos[iM].nPart[1]);
		merit *= (1.0 + 0.00001 * iM);	// We change the merit slightly, 
		//merit *= merit * (1.0 + 0.000001 * iM);	// We change the merit slightly, 
		// to avoid confusion when two halos have the same number of particles and the same number of particles shared with the host halo

		//if (locTask == 0 && idProgenitor.size() > 100)
		//if (locTask == 0) // && idProgenitor.size() > 100)
			//cout << merit << " " << " " << progHalos[iM].nPart[1] << " " << endl;
		//	cout << merit << " " << nCommon[1][iM] << " " << ratioM << " " << progHalos[iM].nPart[1] << " " << endl;

		allMerit.push_back(merit);
	}
	
	idx = SortIndexes(allMerit);
	tmpProgHalos.resize(idx.size());
	tmpIndex.resize(idx.size());
	tmpIdx.resize(idx.size());
	tmpNCommon.resize(nPTypes);

	for (int iT = 0; iT < nPTypes; iT++)
		tmpNCommon[iT].resize(idx.size());

	for (int iM = 0; iM < idProgenitor.size(); iM++)
	{
		/* Inverse sort - from largest to smallest */
		int oldIdx, newIdx;
		oldIdx = idProgenitor.size() - iM - 1;
		newIdx = idx[iM];

		for (int iT = 0; iT < nPTypes; iT++)
			tmpNCommon[iT][oldIdx] = nCommon[iT][newIdx];

		tmpIdx[oldIdx] = idProgenitor[newIdx];
		tmpIndex[oldIdx] = indexProgenitor[newIdx];
		tmpProgHalos[oldIdx] = progHalos[newIdx];
	}	

	for (int iM = 0; iM < idProgenitor.size(); iM++)
	{
		idProgenitor[iM] = tmpIdx[iM];
		indexProgenitor[iM] = tmpIndex[iM];
		progHalos[iM] = tmpProgHalos[iM];

		for (int iT = 0; iT < nPTypes; iT++)
			nCommon[iT][iM] = tmpNCommon[iT][iM];
	}

	tmpIdx.clear();
	tmpIdx.shrink_to_fit();
	tmpIndex.clear();
	tmpIndex.shrink_to_fit();
	tmpProgHalos.clear();
	tmpProgHalos.shrink_to_fit();
};


       /************************************************************************* 
	* These are general functions and are not part of the class Merger Tree *
	*************************************************************************/
		
/* Given two halos, decide whether to compare their particle content or not */
bool CompareHalos(int iHalo, int jHalo, int iOne, int iTwo)
{
	bool order;
	float fVel = 0.4e-2, fDir = 1.0;
	float dX[3], dV[3];
	float min = 100000.0, max = 0.0, dT = 1.0;
	float rMax = 0.0, vMax = 0.0, vOne = 0.0, vTwo = 0.0, dNow = 0.0, dNew = 0.0;
	Halo cmpHalo, thisHalo;
	thisHalo = locHalos[iOne][iHalo];

#ifdef ZOOM
	cmpHalo = locHalos[iTwo][jHalo];

	rMax = thisHalo.rVir + cmpHalo.rVir;
	rMax *= 25.0;	// FIXME check why this works...
#else
	if (jHalo > -1)
		cmpHalo = locHalos[iTwo][jHalo];
	else
		cmpHalo = locBuffHalos[-jHalo-1];	// Compensate with +1 - the indexes are shifted by -1 to avoid overlap with index 0
#endif

	vOne = VectorModule(thisHalo.V);
	vTwo = VectorModule(cmpHalo.V);
	dNow = thisHalo.Distance(cmpHalo.X);
	vMax = (vOne + vTwo);
	
#ifdef TEST // FIXME improve the check on wether halos are moving away or getting closer
		// Also put more constrains using physical velocity and distance...
	/* Linearly extrapolate the distance at the next step to see if the halos are getting closer or further away */
	for (int iX = 0; iX < 3; iX++)
		dX[iX] = (cmpHalo.X[iX] + cmpHalo.V[iX] * dT - thisHalo.X[iX] - thisHalo.V[iX] * dT);

	dNew = VectorModule(dX);

	fDir = (dNew / dNow);	
	//fDir = pow(fDir, 2.0)

	/* In forward mode, iOne = 0 and iTwo = 1. We expect d(iOne) < d(iTwo; and vice versa 
	 * If the two halos are getting away from each other, then reduce the fVel factor */
	if (iOne == 0)
	{
		//cout << locTask << ", " << iHalo << ", old=" << dNow << ", new=" << dNew << " fac:" << fDir << " vmax:" << vMax << endl;
		fVel *= fDir;	// if dNew < dNow, halos are getting closer - increase the search radius
	} else {
		fVel /= fDir;
	}
#endif	
	
	rMax = thisHalo.rVir + cmpHalo.rVir; 

#ifdef ZOOM
	// FIXME check why this works...
	rMax *= 25.0;	
#endif

	rMax *= dMaxFactor * vMax * fVel; 

	// If we are dealing with token halos, enlarge the search radius
	if (thisHalo.isToken || cmpHalo.isToken)
		rMax *= locHalos[iOne][iHalo].nOrphanSteps;
	
	// Only check for pairwise distance
	if (dNow < rMax)
	{
		return true;
	} else {
		return false;
	}
	
};

/* This is a much faster way of comparing particles content of halos across snapshots, that relies on maps 
   and scales linearly with the number of particles on each task */
#ifdef CMP_MAP	
void FindProgenitors(int iOne, int iTwo)
{
	int nLoopHalos[2], iOldOrphans = 0, iFixOrphans = 0, nLocOrphans = 0, nLocTrees = 0; 

	/* Loop also on the buffer halos, in the backward loop only! */
	if (iOne == 1)
	{
		nLoopHalos[iOne] = nLocHalos[iOne] + locBuffHalos.size();
		nLoopHalos[iTwo] = nLocHalos[iTwo]; 
	} else {
		nLoopHalos[iOne] = nLocHalos[iOne];
		nLoopHalos[iTwo] = nLocHalos[iTwo] + locBuffHalos.size();
	}

	locMTrees[iOne].clear();
	locMTrees[iOne].shrink_to_fit();
	locMTrees[iOne].resize(nLoopHalos[iOne]);

	Halo thisHalo;

#ifdef VERBOSE
	if (locTask == 0)
	{
		cout << iOne << ", Loop, " << nLocHalos[iOne] << ",  iTwo " << iTwo << " " << nLocHalos[iTwo] << endl;
		cout << iOne << ", Loc , " << nLoopHalos[iOne] << ",  iTwo " << iTwo << " " << nLoopHalos[iTwo] << endl;
	}
#endif

	/* Reset the tree maps for the inverse comparison */
	if (iOne == 1)
	{
		thisMapTrees.clear();
		nextMapTrees.clear();
	}

	/* Here we initialize (again, for safety and because it's cheap) the maps of halo indexes within the locHalos vectors
	 * and their IDs for faster identification in the loop on the particles */
	for (int iL = 0; iL < nLoopHalos[iOne]; iL++)
	{
		int iH = 0;

		if (iL < nLocHalos[iOne])
		{	
			iH = iL;
			thisHalo = locHalos[iOne][iH];
		} else {
			iH = nLocHalos[iOne]-iL-1;
			thisHalo = locBuffHalos[-iH-1];
		}

		/* Here we link every halo id to its position in the locHalo vector */
		thisMapTrees[thisHalo.ID] = iL;

		locMTrees[iOne][iL].mainHalo = thisHalo; 
		locMTrees[iOne][iL].nCommon.resize(nPTypes);
	}

	/* Map the iTwo halo IDs to their indexes */
	for (int iL = 0; iL < nLoopHalos[iTwo]; iL++)
	{
		int iH = 0;

		if (iL < nLocHalos[iTwo])
		{	
			iH = iL;
			thisHalo = locHalos[iTwo][iH];
		} else {
			iH = nLocHalos[iTwo]-iL-1;
			thisHalo = locBuffHalos[-iH-1];
		}
	
		/* This map connects halo IDs & their indexes in the SECOND locHalo structure */
		nextMapTrees[thisHalo.ID] = iL;
	}

	/* Here we loop on all the particles, each particle keeps track of the Halos it belongs to. 
	 * We match particle IDs in iOne with particle IDs in iTwo, and count the total number of 
	 * particles shared by their two host halos. */
	for (auto const& thisMap : locMapParts[iOne]) 
	{
		unsigned long long int thisID = thisMap.first;		// This particle ID
		vector<Particle> thisParticle = thisMap.second;	 	// How many halos (halo IDs) share this particle

		/* Loop on the halos on iOne to which this particle belongs */
		for (int iH = 0; iH < thisParticle.size(); iH++)
		{
			vector<Particle> nextParticle = locMapParts[iTwo][thisID];
			int thisTreeIndex = thisMapTrees[thisParticle[iH].haloID];
	
			/* This same particle on iTwo is also shared by some halos: do the match with the haloIDs on iOne. */
			for (int iN = 0; iN < nextParticle.size(); iN++)
			{
				unsigned long long int nextHaloID = nextParticle[iN].haloID;

				/* If this ID is not in the list of progenitor IDs, then initialize the indexCommon 
				   map and initialize the number of common particles */
				if (locMTrees[iOne][thisTreeIndex].indexCommon.find(nextHaloID) == 
					locMTrees[iOne][thisTreeIndex].indexCommon.end())
				{
					locMTrees[iOne][thisTreeIndex].indexCommon[nextHaloID].resize(nPTypes);
					locMTrees[iOne][thisTreeIndex].indexCommon[nextHaloID][nextParticle[iN].type] = 1;
				} else {	/* If the Halo ID is already in the index of the halos with common particles,
						   then add ++ to the particle type shared */ 
					locMTrees[iOne][thisTreeIndex].indexCommon[nextHaloID][nextParticle[iN].type]++;
				}
			} // Loop on the halos in the iTwo particles
		} // Loop on the halos in the iOne particles  
	} // Loop on all the iOne particles

	int iOrph = 0;

	/* Once the first loop on the particles is done, we need to fix ALL merger trees
	   moving all the data stored in the map to the "standard" index & id vectors */
	for (int iM = 0; iM < locMTrees[iOne].size(); iM++)
		locMTrees[iOne][iM].AssignMap();

	/* Now clean and reconstruct the local merger trees */
	for (int iM = 0; iM < locMTrees[iOne].size(); iM++)
	{
		unsigned long long int thisHaloID;
		int nProgs = locMTrees[iOne][iM].idProgenitor.size();
		int thisHaloIndex = 0;

		//if (locMTrees[iOne][iM].isOrphan && locTask == 0)
		//	cout << iM << ", " << nProgs << ", " << thisHaloIndex << ", " 
		//		 << locMTrees[iOne][thisHaloIndex].idProgenitor.size() << ", ORPHAN." << endl; 

		for (int iP = 0; iP < nProgs; iP++)
		{
			thisHaloID = locMTrees[iOne][iM].idProgenitor[iP];
			thisHaloIndex = nextMapTrees[thisHaloID];			

			/* Here we fill the indexProgenitor & progHalos */
			locMTrees[iOne][iM].indexProgenitor[iP] = thisHaloIndex;

			if (thisHaloIndex >= nLocHalos[iTwo])
				locMTrees[iOne][iM].progHalos[iP] = locBuffHalos[thisHaloIndex-nLocHalos[iTwo]];
			else
				locMTrees[iOne][iM].progHalos[iP] = locHalos[iTwo][thisHaloIndex];
		}

		locMTrees[iOne][iM].SortByMerit();

		/* Orphan halos are identified in the forward search only */
		if (iOne == 0)
		{
			if (locMTrees[iOne][iM].idProgenitor.size() == 0 && 
				locMTrees[iOne][iM].mainHalo.nPart[1] > minPartHalo)
			{

				Halo thisHalo = locHalos[iOne][iM];
				thisHalo.isToken = true;
				thisHalo.nOrphanSteps++;

				if (thisHalo.nOrphanSteps > 1)
					iOldOrphans++;

				/* Update the container of local orphan halos */
				locOrphIndex.push_back(iM);
				locOrphHalos.push_back(thisHalo);

				/* Update the local mtree with a copy of itself */
				locMTrees[iOne][iM].isOrphan = true;
				locMTrees[iOne][iM].idProgenitor.push_back(locHalos[iOne][iM].ID);

				/* Update the particle content */
				locOrphParts.push_back(locParts[iOne][iM]);
				locOrphParts[nLocOrphans].resize(nPTypes);

				for (int iP = 0; iP < nPTypes; iP++)
					copy(locParts[iOne][iM][iP].begin(), locParts[iOne][iM][iP].begin(), 
						back_inserter(locOrphParts[nLocOrphans][iP]));

				nLocOrphans++;
			} else {

				if (locHalos[iOne][iM].isToken)
					iFixOrphans++;

				locMTrees[iOne][iM].isOrphan = false;
				nLocTrees++;
			}
		} // if iOne == 0
	}



	/* Trace the orphans in the forward loop */
	if (iOne == 0)
	{
		nLocOrphans = locOrphHalos.size();
		int nTotOrphans = 0, nTotFix = 0, nTotOld = 0, nTotTrees = 0; 

		//cout << "\nFound " << nLocOrphans << " orphan halos on task " << locTask << ", " << locOrphIndex.size() << endl;
		MPI_Reduce(&nTotTrees, &nLocTrees, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
		MPI_Reduce(&nLocOrphans, &nTotOrphans, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
		MPI_Reduce(&iOldOrphans, &nTotOld, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
		MPI_Reduce(&iFixOrphans, &nTotFix, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

		if (locTask == 0)
		{	
			cout << endl;
			cout << "  Tracking a total of " << nTotTrees << " on " << totTask << " tasks. "  << endl;
			cout << "  On all tasks, there are: " << endl;
			cout << "     ---> " << nTotOrphans << " total orphan halos." << endl;
			cout << "     ---> " << nTotOrphans - nTotOld << " new orphan halos." << endl;
			cout << "     ---> " << nTotOld << " orphans since more than one step." << endl;
			cout << "     ---> " << nTotFix << " orphans that have been reconnected to their descendants.  " << endl;
			cout << "  On the master task there are " << nLocTrees << " as well as: " << endl;
			cout << "     ---> " << nLocOrphans << " total orphan halos." << endl;
			cout << "     ---> " << nLocOrphans - iOldOrphans << " new orphan halos." << endl;
			cout << "     ---> " << iOldOrphans << " orphans since more than one step." << endl;
			cout << "     ---> " << iFixOrphans << " orphans that have been reconnected to their descendants.  " << endl;
		}
	}

};

#else	/* This is using the standard implementation of the comparison algorithm, which is much slower */

/* Find all the progenitors (descendants) of the halos in catalog iOne (iTwo) */
void FindProgenitors(int iOne, int iTwo)
{

	vector<int> thisNCommon, indexes, totNCommon;
	float rSearch = 0, facRSearch = 40.0, radiusSearchMax = 5000.0;		//TODO find a better way to implement facRSearch
	int nStepsCounter = floor(nLocHalos[iUseCat] / 50.);
	int totCmp = 0, totSkip = 0, nLocOrphans = 0; 
	int nLoopHalos = 0, iOldOrphans = 0, iFixOrphans = 0;
	
	nLoopHalos = nLocHalos[iOne];

#ifndef ZOOM
	/* Loop also on the buffer halos, in the backward loop only! */
	if (iOne > iTwo)
		nLoopHalos = nLocHalos[iOne] + locBuffHalos.size();
#endif

	locMTrees[iOne].clear();
	locMTrees[iOne].shrink_to_fit();
	locMTrees[iOne].resize(nLoopHalos);

	//cout << "OnTask=" << locTask << " nHalos: " << nLocHalos[iOne] << " nHalos+nBuff: " << nLoopHalos << 
	//	" locBuffSize: " << locBuffHalos.size() << " MT:" << locMTrees[iOne].size() << endl;


#ifdef VERBOSE
	cout << "\nOnTask=" << locTask << " nHalos: " << nLocHalos[iOne] << " nHalos+nBuff: " << nLoopHalos << 
		" locBuffSize: " << locBuffHalos.size() << " MT:" << locMTrees[iOne].size() << endl;
#endif



#ifdef ZOOM
			/**************************************************
			 *	Comparison for zoom simulations           *
			 **************************************************/

	vector<int> locHaloPos;
	int thisIndex = 0, halosPerTask = 0, halosRemaind = 0;

	/* For consistency across tasks and to keep track of all possible orphans, IDs need to be initialized here */
	for (int iT = 0; iT < locHalos[iOne].size(); iT++)
		locMTrees[iOne][iT].mainHalo = locHalos[iOne][iT];

	/* The way forward/backward comparisons are done is different: in the forward mode, halos are split among the tasks as 
	 * Task 1 --> halo 1, task 2 ---> halo 2, assuming that the halos are ordered with increasing (decreasing) number of particles.
	 * In the backward mode, we assign to each task only those halos which have been matched in the fwd step, to avoid unnecessary
	 * comparisons.   */
	if (iOne < iTwo)	
	{
		halosPerTask = int(nLocHalos[iOne] / totTask);
		halosRemaind = nLocHalos[iOne] % totTask;
		locTreeIndex.clear();
		locTreeIndex.shrink_to_fit();

		/* The first halosRemaind tasks get one halo more */
		if (locTask < halosRemaind)
			halosPerTask += 1;

		if (locTask == 0)
			cout << "Looping on " << halosPerTask << " halos in the forward loop." << endl;
	} else {

		/* If doing backward correlation we have to loop on all halos (ZOOM mode) */
		halosPerTask = locTreeIndex.size();
		
		if (locTask == 0)
			cout << "Looping on " << halosPerTask << " halos in the backward loop." << endl;
	}

	for (int iH = 0; iH < halosPerTask; iH++)
	{
		/* This distribution assumes that halos are ordered per number of particles, to improve the load balancing */
		if (iOne < iTwo)
			thisIndex = locTask + iH * totTask;
		else
			thisIndex = locTreeIndex[iH];

		Halo thisHalo = locHalos[iOne][thisIndex];
		
		/* Save some halo properties on the mtree vector */
		locMTrees[iOne][thisIndex].mainHalo = thisHalo;	//TODO isn't this redndant???
		locMTrees[iOne][thisIndex].nCommon.resize(nPTypes);
	
			/* In a zoom-in run, we loop over all the halos on the iTwo step */
			for (int jH = 0; jH < locHalos[iTwo].size(); jH++)
			{ 
				thisNCommon.resize(nPTypes);
                                //if (CompareHalos(i, j, iOne, iTwo))	// TODO this does not really improve speed, need to do more tests
				{
					thisNCommon = CommonParticles(locParts[iOne][thisIndex], locParts[iTwo][jH]);
	
					int totComm = 0;
					totComm = thisNCommon[0] + thisNCommon[1] + thisNCommon[2];

					/* This is very important: we keep track of the merging history ONLY if the number 
					 * of common particles is above a given threshold */
					if (totComm > minPartCmp) 
					{	
						/* Common particles are separated per particle type */	
						for (int iT = 0; iT < nPTypes; iT++)
						{	
							//cout << j << ", " << iT << ", " << thisNCommon[iT] << endl;
							locMTrees[iOne][thisIndex].nCommon[iT].push_back(thisNCommon[iT]);
						}

						locMTrees[iOne][thisIndex].idProgenitor.push_back(locHalos[iTwo][jH].ID);
						locMTrees[iOne][thisIndex].indexProgenitor.push_back(jH);
					
						/* If the two halos have the same ID, we are dealing with a token halo
						 * placed to trace the progenitor of an orphan halo */
						if (locMTrees[iOne][thisIndex].mainHalo.ID == locHalos[iTwo][jH].ID)
							locMTrees[iOne][thisIndex].isOrphan == true;

						/* We keep track of the halos on iTwo that have been matched on iOne on the 
						 * local task, so that we can avoid looping on all iTwo halos afterwards */
						if (iOne < iTwo)
						{
							locTreeIndex.push_back(jH);
							if (thisHalo.isToken)
								iFixOrphans++;

						totCmp++;
					}
				} // CompareHalos
				
				thisNCommon.clear();
				thisNCommon.shrink_to_fit();
			}

			locMTrees[iOne][thisIndex].SortByMerit();

			/* Very important: if it turns out the halo has no likely progenitor, and has a number of particles above 
			 * minPartHalo, then we keep track of its position in the global locHalos[iOne] array  */
			if (locMTrees[iOne][thisIndex].idProgenitor.size() == 0 && 
				locMTrees[iOne][thisIndex].mainHalo.nPart[1] > minPartHalo && iOne < iTwo)
				{
					for (int iT = 0; iT < nPTypes; iT++)
						locMTrees[iOne][thisIndex].nCommon[iT].push_back(locHalos[iOne][thisIndex].nPart[iT]);

					orphanHaloIndex.push_back(thisIndex);
					locMTrees[iOne][thisIndex].isOrphan = true;
				} else {
					locMTrees[iOne][thisIndex].isOrphan = false;
				}

#ifdef DEBUG		// Sanity check
			if (locMTrees[iOne][thisIndex].nPart > 1000)
				locMTrees[iOne][thisIndex].Info();
#endif
	}	// End loop on iOne halo

	/* Sort and clean of multiple entries the locTreeIndex */
	sort(locTreeIndex.begin(), locTreeIndex.end());
	locTreeIndex.erase(unique(locTreeIndex.begin(), locTreeIndex.end()), locTreeIndex.end());

			/**************************************************
			 *	Comparison for fullbox simulations        *
			 **************************************************/
#else						

		for (int iL = 0; iL < nLoopHalos; iL++)
		{
			int iH = 0;
			Halo thisHalo;
			totCmp = 0; totSkip = 0; 

			if (iL < nLocHalos[iOne])
			{	
				iH = iL;
				thisHalo = locHalos[iOne][iH];
			} else {
				iH = nLocHalos[iOne]-iL-1;
				thisHalo = locBuffHalos[-iH-1];
			}

			locMTrees[iOne][iL].mainHalo = thisHalo; 
			locMTrees[iOne][iL].nCommon.resize(nPTypes);

			if (iL == nStepsCounter * floor(iL / nStepsCounter) && locTask == 0)
					cout << "." << flush; 

			rSearch = radiusSearchMax; 

			/* We only loop on a subset of halos */
			indexes = GlobalGrid[iTwo].ListNearbyHalos(thisHalo.X, rSearch);

			// TODO this loop could be speeded up a little bit if we skip the comparisons of token halos 
			// in the 1 --> 0 (backwards) loop, but maybe it's not even worth it...
			/* The vector "indexes" contains the list of haloes (in the local memory & buffer) to be compared */
			for (int jH = 0; jH < indexes.size(); jH++)
			{
				int kH = indexes[jH];
				bool compCondition;

				/* The comparison condition is symmetric in the two indexes, 
				 * so invert it in the case of backward comparison to take 
				 * into account the fact that the sign might be negative in iH */
				if (iOne < iTwo)
					compCondition = CompareHalos(iH, kH, iOne, iTwo);
				else
					compCondition = CompareHalos(kH, iH, iTwo, iOne);

				/* Compare halos --> this functions checks whether the two halos are too far 
				   or velocities are oriented on opposite directions */
				if (compCondition)
				{	
					int totComm = 0;
		
					// These two indexes should NEVER be negative at the same time. We only have one buffer
					if (kH < 0 && iH < 0)  
						cout << "ERROR. Two negative indexes on task=" << locTask << " kH= " << kH 
							<< " iH = " << iH << ". Only one negative index allowed. "  
							<< " buffer size=" << locBuffParts.size() << endl;
					if (kH > -1 && iH > -1)
						thisNCommon = CommonParticles(locParts[iOne][iH], locParts[iTwo][kH]);
					else if (kH < 0)
						thisNCommon = CommonParticles(locParts[iOne][iH], locBuffParts[-kH-1]);
					else if (iH < 0)
						thisNCommon = CommonParticles(locBuffParts[-iH-1], locParts[iTwo][kH]);

					totComm = thisNCommon[0] + thisNCommon[1] + thisNCommon[2];
						
					if (totComm > 0)
						totCmp++;

					/* This is very important: we keep track of the merging history ONLY if the number 
					 * of common particles is above a given threshold */
					if (totComm > minPartCmp) 
					{		
						//if (iL > nLocHalos[iOne])
						//	cout << " There are " << iL << " halos on task " << locTask << endl; 

						for (int iT = 0; iT < nPTypes; iT++)
							locMTrees[iOne][iL].nCommon[iT].push_back(thisNCommon[iT]);
	
						// In the backward comparison kH is only on the task halos, always > 0
						if (kH > -1)	
						{
							locMTrees[iOne][iL].idProgenitor.push_back(locHalos[iTwo][kH].ID);
							locMTrees[iOne][iL].progHalos.push_back(locHalos[iTwo][kH]);
						} else { 
							locMTrees[iOne][iL].idProgenitor.push_back(locBuffHalos[-kH-1].ID);	
							locMTrees[iOne][iL].progHalos.push_back(locHalos[iTwo][-kH-1]);
						}

						locMTrees[iOne][iL].indexProgenitor.push_back(kH);
							
							if (thisHalo.isToken)
								iFixOrphans++;
					
					} else {
						totSkip++;
					}
					
				} 	// if Halo Comparison
			}	// for j, k = index(j)

		locMTrees[iOne][iL].SortByMerit();

		/* Very important check! Check for orphans only in the fwd loop to avoid segfaults */
		if (iOne == 0)
			/* Very important: if it turns out the halo has no likely progenitor, and has a number of particles above 
			 * minPartHalo, then we add it to the grid & the iTwo step & the iTwo grid */
			if (locMTrees[iOne][iH].idProgenitor.size() == 0 && 
				locMTrees[iOne][iH].mainHalo.nPart[1] > minPartHalo)
				{
					Halo thisHalo = locHalos[iOne][iH];
					thisHalo.isToken = true;
					thisHalo.nOrphanSteps++;

					if (thisHalo.nOrphanSteps > 1)
						iOldOrphans++;
					//cout << iL << " iOne: "<< iOne << ", Token n steps = " << thisHalo.nOrphanSteps << endl; 

					/* Update the container of local orphan halos */
					locOrphIndex.push_back(iH);
					locOrphHalos.push_back(thisHalo);

					/* Update the local mtree with a copy of itself */
					locMTrees[iOne][iH].isOrphan = true;
					locMTrees[iOne][iH].idProgenitor.push_back(locHalos[iOne][iH].ID);

					/* Update the particle content */
					locOrphParts.push_back(locParts[iOne][iH]);
					locOrphParts[nLocOrphans].resize(nPTypes);

					for (int iP = 0; iP < nPTypes; iP++)
						copy(locParts[iOne][iH][iP].begin(), locParts[iOne][iH][iP].begin(), 
							back_inserter(locOrphParts[nLocOrphans][iP]));

					nLocOrphans++;
				} else {
					locMTrees[iOne][iH].isOrphan = false;
				}

		} // for i halo, the main one

		if (iOne == 0)
		{
			nLocOrphans = locOrphHalos.size();
			int nTotOrphans = 0, nTotFix = 0, nTotOld = 0; 

			//cout << "\nFound " << nLocOrphans << " orphan halos on task " << locTask << ", " << locOrphIndex.size() << endl;
			MPI_Reduce(&nLocOrphans, &nTotOrphans, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
			MPI_Reduce(&iOldOrphans, &nTotOld, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
			MPI_Reduce(&iFixOrphans, &nTotFix, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

			if (locTask == 0)
				cout << "\nFound " << iOldOrphans << " old, " << nLocOrphans << " new, " << iFixOrphans << 
					" fixed orphan halos. Total " << nTotOld << " old, " << nTotFix << " fixed and "
					<< nTotOrphans << " total on all tasks. " << endl;

		}
#endif		// ifdef ZOOM
};
#endif		// ifdef CMP_MAP


/* Given a pair of haloes, determine the number of common particles */
vector<int> CommonParticles(vector<vector<unsigned long long int>> partsHaloOne, 
	vector<vector<unsigned long long int>> partsHaloTwo)
{
	vector<int> nCommon; 
	vector<unsigned long long int>::iterator iter;
	vector<unsigned long long int> thisCommon;

	nCommon.resize(nPTypes);	

	for (int iT = 0; iT < nPTypes; iT++)
	{
		int oneSize = partsHaloOne[iT].size();
		int twoSize = partsHaloTwo[iT].size();

		if (twoSize > 0)
		{
			// This is the maximum possible number of common particles
			thisCommon.resize(twoSize);
	
			// Find out how many particles are shared among the two arrays
			iter = set_intersection(partsHaloOne[iT].begin(), partsHaloOne[iT].end(), 
					partsHaloTwo[iT].begin(), partsHaloTwo[iT].end(), thisCommon.begin());	

			// Resize the array and free some memory
			thisCommon.resize(iter - thisCommon.begin());

			// Now compute how many particles in common are there
			nCommon[iT] = thisCommon.size();

			// Clear the vector and free all the allocated memory
			thisCommon.clear();
	 		thisCommon.shrink_to_fit();
		}

	}
	
	return nCommon;

};



void InitTrees(int nUseCat)
{
	locCleanTrees.resize(nUseCat-1);
};


/* This function compares the forward/backward connections to determine the unique descendant of each halo
 * TODO in full box mode, add the possibility to check the halos in the buffer and remove overlapping objects 
 * TODO Need to establish a criterion to determine to which task the halo should belong to */
void CleanTrees(int iStep)
{
	int thisIndex = 0, halosPerTask = 0, halosRemaind = 0;
	int nErr = 0;
        halosPerTask = int(nLocHalos[0] / totTask);
        halosRemaind = nLocHalos[0] % totTask;

#ifdef ZOOM
	if (locTask < halosRemaind)
		halosPerTask += 1;
#else
	halosPerTask = nLocHalos[0]; //locMTrees[0].size();
#endif

	if (locTask == 0)
		cout << "Cleaning Merger Tree connections, back and forth, for " << halosPerTask << " halos." << endl;

#ifdef VERBOSE
	if (locTask == 0)
		cout << "nHalos " << locHalos[0].size() << ", nTrees: " << locMTrees[0].size() << endl; 
#endif

	for (int kTree = 0; kTree < halosPerTask; kTree++)
	{

#ifdef ZOOM	// In ZOOM mode each task holds all the halos, but only analyzes some of them
		int iTree = locTask + kTree * totTask;
#else
		int iTree = kTree;
#endif
		unsigned long long int mainID = locHalos[0][iTree].ID;
		int nProgSize = locMTrees[0][iTree].idProgenitor.size();

		MergerTree mergerTree;
		mergerTree.nCommon.resize(nPTypes);
		mergerTree.mainHalo = locHalos[0][iTree];
		mergerTree.isOrphan = locMTrees[0][iTree].isOrphan;

		if (mergerTree.isOrphan)
		{
			nProgSize = 0;
			locMTrees[0][iTree].idProgenitor[0] = mainID;
			mergerTree.idProgenitor.push_back(mainID);
			mergerTree.progHalos.push_back(locHalos[0][iTree]);
			
			for (int iT = 0; iT < nPTypes; iT++)
			{
				mergerTree.nCommon[iT].resize(1);
				mergerTree.nCommon[iT][0] = locHalos[0][iTree].nPart[iT];
			}
		}

		/* At each step we only record the connections between halos in catalog 0 and catalog 1, without attempting at a
		 * reconstruction of the full merger history. This will be done later. */
		for (int iProg = 0; iProg < nProgSize; iProg++)
		{
			Halo progHalo;	
			int jTree = locMTrees[0][iTree].indexProgenitor[iProg];
			unsigned long long int progID = locMTrees[0][iTree].idProgenitor[iProg];
			unsigned long long int descID;

#ifndef ZOOM 		
			if (jTree < 0) 
			{
				int kTree = -jTree -1;			// Need to correct for the "offset" factor

				if (kTree > locMTrees[1].size())
					cout << locTask << ", " <<  jTree << ", " << nLocHalos[1] << ", " << locMTrees[1].size() << endl;

				/* This kind of error might be due to the incorrect setting of the facRSearch variable */
				if(locMTrees[1][kTree + nLocHalos[1]].idProgenitor.size() == 0)
				{
					cout << "ERROR OnTask:" <<  locTask << ", jTree:" <<  jTree 
						<< ", nHalos:" << nLocHalos[1] << ", locTrees:" 
							<< locMTrees[1][kTree+nLocHalos[1]].progHalos.size() << endl;
	
					progHalo.Info();
					mergerTree.mainHalo.Info();
					locMTrees[1][kTree + nLocHalos[1]+1].mainHalo.Info();

					//cout << "index: " << locMTrees[1][nLocHalos[1]-jTree].indexProgenitor.size() <<
					//" id: " << locMTrees[1][nLocHalos[1]-jTree].idProgenitor.size() << endl;	
				} else {
					progHalo = locMTrees[1][nLocHalos[1]+kTree].mainHalo;	
					descID = locMTrees[1][nLocHalos[1]+kTree].idProgenitor[0];	
				}
		
			} else {
				
				// Sanity check
				if (jTree > locMTrees[1].size())
					cout << "ERROR in CleanTrees(). MTree size: " << locMTrees[1].size() 
						<< ", indexj: " << jTree << endl;
						
				if (locMTrees[1][jTree].idProgenitor.size() > 0)
				{
					descID = locMTrees[1][jTree].idProgenitor[0];	
				} else {
					locMTrees[1][jTree].mainHalo.Info();
				}

				progHalo = locMTrees[1][jTree].mainHalo;
			}
#else
			descID = locMTrees[1][jTree].idProgenitor[0];	
			progHalo = locMTrees[1][jTree].mainHalo;
#endif

			/* Sanity check */
			if (descID == 0 && progHalo.nPart[1] > minPartHalo)
			{
				cout << "WARNING. Progenitor ID not assigned: " << progID << " " << descID 
					<< " | " << iTree << " " << jTree << endl;
			}

			if (mainID == descID)
			{
				mergerTree.idProgenitor.push_back(progID);
				mergerTree.indexProgenitor.push_back(jTree);
				mergerTree.progHalos.push_back(progHalo);

				for(int iT = 0; iT < nPTypes; iT++)
					mergerTree.nCommon[iT].push_back(locMTrees[0][iTree].nCommon[iT][iProg]);
			}	// mainID = descID
		}	// iProg for loop

		if (!mergerTree.isOrphan)	
			mergerTree.SortByMerit();

		if (mergerTree.idProgenitor.size() > 0)
			locCleanTrees[iStep-1].push_back(mergerTree);

		mergerTree.Clean();
	}	// kTree for loop
	
	//cout << "OnTask = " << locTask << " n errors = " << nErr << endl;

#ifdef ZOOM
	locTreeIndex.clear();
	locTreeIndex.shrink_to_fit();
#endif
};



/* These two functions are used in mode = 1, when the MTrees are being read in from the .mtree files */
void AssignDescendant()
{
	unsigned long long int mainID = 0;
	int mainIndex = 0;

#ifdef ZOOM
	orphanHaloIndex.clear();
	orphanHaloIndex.shrink_to_fit();
#else
	locOrphIndex.clear();
	locOrphIndex.shrink_to_fit();
	locOrphHalos.clear();
	locOrphHalos.shrink_to_fit();
#endif

	//cout << "AssignDescendant(): " << locCleanTrees[iNumCat-1].size() << endl;

	for (int iC = 0; iC < locCleanTrees[iNumCat-1].size(); iC++)
	{
		mainID = locCleanTrees[iNumCat-1][iC].mainHalo.ID;
		mainIndex = id2Index[mainID];

		if (id2Index.find(mainID) != id2Index.end()) 
		{
			if (locCleanTrees[iNumCat-1][iC].isOrphan)
			{
#ifdef ZOOM
				orphanHaloIndex.push_back(mainIndex);
#else
				locOrphIndex.push_back(mainIndex);
				locOrphHalos.push_back(locCleanTrees[iNumCat-1][iC].mainHalo);
#endif
			}

			locCleanTrees[iNumCat-1][iC].mainHalo = locHalos[iUseCat][mainIndex];
		} else {
			cout << locTask << " does not have descendant ID: " << mainID << endl;
		}
	}

	/* Halos have been assigned, so we can clear the map */
	id2Index.clear();	
};



void AssignProgenitor()
{
	unsigned long long int progID = 0;
	int progIndex = 0;

	orphanHaloIndex.clear();
	orphanHaloIndex.shrink_to_fit();

	for (int iC = 0; iC < locCleanTrees[iNumCat-1].size(); iC++)
	{
		for (int iS = 0; iS < locCleanTrees[iNumCat-1][iC].progHalos.size(); iS++ )
		{
			progID = locCleanTrees[iNumCat-1][iC].progHalos[iS].ID;
	
			if(locCleanTrees[iNumCat-1][iC].isOrphan)
			{
				locCleanTrees[iNumCat-1][iC].progHalos[0] = locCleanTrees[iNumCat-1][iC].mainHalo; 
			} else {

				if (id2Index.find(progID) != id2Index.end()) 
				{
					progIndex = id2Index[progID];
	
					if (progIndex > -1)
						locCleanTrees[iNumCat-1][iC].progHalos[iS] = locHalos[iUseCat][progIndex];
					else
						locCleanTrees[iNumCat-1][iC].progHalos[iS] = locBuffHalos[-progIndex-1];

				} else {
					//cout << locTask << " does not have progenitor ID: " << progID << endl;
					//locCleanTrees[iNumCat-1][iC].progHalos[iS].Info();
				}
			}
		}
	}

	/* Halos have been assigned, so we can clear the map */
	id2Index.clear();	
};



void DebugTrees()
{
	if (locTask == 0)
		cout << "Debugging trees for steps=" << locCleanTrees.size() << endl;
 
	for (int iC = 0; iC < locCleanTrees.size(); iC++)
	{	
		cout << "Task=(" << locTask << ") Size=(" << locCleanTrees[iC].size() << ") Step=(" << iC << ") " << endl;

		for (int iT = 0; iT < locCleanTrees[iC].size(); iT++)
				locCleanTrees[iC][iT].Info();
	}
};




