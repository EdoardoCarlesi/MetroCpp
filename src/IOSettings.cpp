#include <string>
#include <sstream>
#include <fstream>
#include <algorithm>

#include <array>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>

#include "IOSettings.h"
#include "general.h"

using namespace std;


IOSettings::IOSettings() 
{
};


IOSettings::~IOSettings() 
{
};


void IOSettings::FindCatID()
{	
	string outputSh;
	string inputSh;
	string optionsSh;
	string outputTmp;
	string lineIn;
	string cleanTmp;
	char *tmpLine;
	int iS = 0, sysOut = 0;

	optionsSh = pathInput + " " + catFormat;
	outputTmp = thisPath + tmpIdOut;
	inputSh = thisPath + findIDsh + " " + optionsSh + " > " + outputTmp;
	cout << inputSh << endl;	

	if(FILE *f = fopen(outputTmp.c_str(), "r"))
	{
		cout << "File " << outputTmp << " found. " << endl;

	} else {

		// Execute the bash script to find out the snapshot IDs. These are usually just the snapshot numbers, 
		// but might change sometimes if there is a "hole" in between.
		cout << inputSh << endl;
		sysOut = system(inputSh.c_str());
	}

	ifstream fileIn(outputTmp);
	
	// Read the catalog IDs generated by the catalog	
	while (getline(fileIn, lineIn))
	{
		strSnaps[nCat - iS -1] = lineIn.c_str(); 
		numSnaps[nCat - iS -1] = stoi(lineIn.c_str()); 
		iS++;
	}

#ifdef CLEAN_TMP
	cleanTmp = "rm " + outputTmp;
	sysOut = system(cleanTmp.c_str());
#endif
};


void IOSettings::FindCatZ()
{	
	int iZ = 0, sysOut = 0;
	string outputSh;
	string inputSh;
	string optionsSh;
	string outputTmp;
	string cleanTmp;
	string lineIn;

	optionsSh = pathInput + " " + catFormat;
	outputTmp = thisPath + tmpZOut;
	inputSh = thisPath + findZsh + " " + optionsSh + " > " + outputTmp;
	cout << inputSh << endl;	

	if(FILE *f = fopen(outputTmp.c_str(), "r"))
	{
		cout << "File " << outputTmp << " found. " << endl;

	} else {
	
		// Execute the bash script and find out the redshifts of the snapshot files.
		// TODO this assumes AHF format! Other formats might not dump the z value in the output file
		cout << inputSh << endl;
		sysOut = system(inputSh.c_str());
	}

	ifstream fileIn(outputTmp);
		
	// Read the redshifts generated by the catalog
	while (getline(fileIn, lineIn))
	{
		const char *lineRead = lineIn.c_str();
		float thisZ = 0.0, thisA = 0.0;

		sscanf(lineRead, "%f", &thisZ);
		thisA = 1.0 / (1.0 + thisZ) ;

		redShift[nCat - iZ - 1] = thisZ;
		aFactors[nCat - iZ - 1] = thisA;

		//cout << nCat-iZ-1 << ", z = " << redShift[nCat - iZ -1] << ", a=" << aFactors[nCat - iZ -1]<< endl;
		iZ++;
	}
	
#ifdef CLEAN_TMP
	// Remove temporary files
	cleanTmp = "rm " + outputTmp;
	sysOut = system(cleanTmp.c_str());
#endif
};


void IOSettings::FindCatN()
{	
	int sysOut = 0;
	string outputSh;
	string inputSh;
	string optionsSh;
	string outputTmp;
	string lineIn;
	string cleanTmp;

	optionsSh = pathInput + " " + catFormat;
	outputTmp = thisPath + tmpNOut;
	inputSh = thisPath + findNsh + " " + optionsSh + " > " + outputTmp;
	cout << inputSh << endl;

	if(FILE *f = fopen(outputTmp.c_str(), "r"))
	{
		cout << "File " << outputTmp << " found. " << endl;
	} else {

		sysOut = system(inputSh.c_str());
	}

		ifstream fileIn(outputTmp);
		
	// Read the number of catalogs generated by the script
	while (getline(fileIn, lineIn))
	{
		const char *lineRead = lineIn.c_str();
		sscanf(lineRead, "%d", &nCat);
	}

#ifdef CLEAN_TMP	
	// Remove temporary files
	cleanTmp = "rm " + outputTmp;
	sysOut = system(cleanTmp.c_str());
#endif
};


void IOSettings::Init()
{
	// Use only one task to read the files
	if (locTask == 0)
		FindCatN();

	// Once the catalog number has been found, communicate it to all tasks
	MPI_Bcast(&nCat, 1, MPI_INT, 0, MPI_COMM_WORLD);

	// Allocate memory for the catalog names, numbers, redshifts and a factors
	redShift.resize(nCat);
	aFactors.resize(nCat);
	strSnaps.resize(nCat);
	numSnaps.resize(nCat);

	// Now read the catalog names on the master task, broadcast everything later
	if (locTask == 0)
	{
		cout << "Found " << nCat << " redshifts in total." << endl;

		FindCatID();
		FindCatZ();
	}

	MPI_Bcast(&redShift[0], nCat, MPI_FLOAT, 0, MPI_COMM_WORLD);
	MPI_Bcast(&aFactors[0], nCat, MPI_FLOAT, 0, MPI_COMM_WORLD);
	MPI_Bcast(&numSnaps[0], nCat, MPI_INT, 0, MPI_COMM_WORLD);

	// Convert integerts to snapshot strings on each task, it is easier than MPI_Bcast all those chars
	if (locTask != 0)
	{
		// The snapshot format is _XXX so we assume 4 char. BEWARE! Other formats might not be compatible
		char strBuff[4];

		for (int i = 0; i < nCat; i++)
		{
			sprintf(strBuff, "%03d", numSnaps[i]); 
			strSnaps[i] = strBuff;
		}
	}

};



void IOSettings::DistributeFilesAmongTasks(void)
{
	char charCpu[5], charZ[8];
	int locChunk = 0;
	string strZ;

	// TODO This function should optimize the halo distribution es. giving each task a slab of haloes, which 
	// would also make the FFTW operation much faster, if planning to implement a purely PM grid to compute 
	// the gravitational force at each step

	nFilesOnTask = nChunksPerFile; // / totTask;

	if (locTask == 0)
		cout << "Assigning " << nFilesOnTask << " halo/particle files among " << totTask << " tasks. " << endl; 

	haloFiles.resize(nCat);
	partFiles.resize(nCat);

	for (int i = 0; i < nCat; i++)
	{
		sprintf(charZ, "%.3f", redShift[i]);	
		haloFiles[i].resize(nFilesOnTask);
		partFiles[i].resize(nFilesOnTask);

		for (int j = 0; j < nFilesOnTask; j++)
		{
			locChunk = j + locTask * nFilesOnTask;
			sprintf(charCpu, "%04d", locChunk);	
			haloFiles[i][j] = pathInput + haloPrefix + strSnaps[i] + "." + charCpu + ".z" + charZ + "." + haloSuffix;
			partFiles[i][j] = pathInput + haloPrefix + strSnaps[i] + "." + charCpu + ".z" + charZ + "." + partSuffix;

			ifstream haloExists(haloFiles[i][j]);
			if (haloExists.fail())
				cout << "WARNING: on task =" << locTask << " " << haloFiles[i][j] << " not found." << endl;

			ifstream partExists(haloFiles[i][j]);
			if (partExists.fail())
				cout << "WARNING: on task =" << locTask << " " << haloFiles[i][j] << " not found." << endl;
		
			//if (locTask == 0 || locTask == 2)  
			//	cout << locTask << ") haloFiles[" << i << "][" << j << "]: " << haloFiles[i][j] << endl; 

		}
	}
};


// Particle sizes have already been allocated in the ReadHalos() routines, do a safety check for the size
// TODO use read(buffer,size) to read quickly blocks of particles all at the same time 
void IOSettings::ReadParticles(void)
{
	string tmpStrUrlPart;
	const char *tmpUrlPart;
	unsigned long long int locHaloID = 0, partID = 0;
	unsigned int iTmpParts = 0, totLocParts = 0, iLine = 0, nPartHalo = 0;
	unsigned int nFileHalos = 0, iLocHalos = 0, iTmpHalos = 0, nTmpHalos = 0;
	int partType = 0, nChunks = 0;
	string lineIn;

	nChunks = nChunksPerFile;
		
	for (int iChunk = 0; iChunk < nChunks; iChunk++)
	{
		tmpUrlPart = partFiles[iNumCat][iChunk].c_str();
		ifstream fileIn(tmpUrlPart);

		// Reset temporary variables
		iTmpParts = 0;
		iTmpHalos = 0;
		iLine = 0;

		if (!fileIn.good())
		{
			cout << "File: " << tmpUrlPart << " not found on task=" << locTask << endl;
		} else {
			if (locTask == 0)
	        		cout << "Reading " << nLocParts << " particles from file: " << tmpUrlPart << endl;
		}

		while (getline(fileIn, lineIn))
		{
			const char *lineRead = lineIn.c_str();		

			// TODO this is all AHF format dependent
			if (iLine == 0)
			{
		                sscanf(lineRead, "%d", &nFileHalos);
				iLine++;

			} else if (iLine == 1) {

		                sscanf(lineRead, "%u %llu", &nPartHalo, &locHaloID);
				tmpParts.resize(nPTypes+1);
				iLine++;

			} else {

	        	        sscanf(lineRead, "%llu %hd", &partID, &partType);
				tmpParts[iLocHalos][partType].push_back(partID);
				totLocParts++;
				iTmpParts++;

				if (iTmpParts == locHalos[iUseCat][iLocHalos].nPart[nPTypes])
				{	
					// Sort the ordered IDs
					for (int iT = 0; iT < nPTypes; iT++)
					{	
						if (tmpParts[iTmpHalos][iT].size() > 0)
						{
							sort(tmpParts[iT].begin(), tmpParts[iT].end());
						
						locParts[iUseCat][iLocHalos][iT].insert(tmpParts[iUseCat].end(), tmpParts[iTmpHalos][iT].begin(), tmpParts[iT].end());
						}
					}

					iLocHalos++;
					iLine = 1;	// Reset some indicators
				}
			} // else iLine not 0 or 1
		} // end while

		//locParts[iUseCat].insert(locParts[iUseCat].end(), tmpParts[iUseCat].begin(), tmpParts[iUseCat].end());

	} // End for loop on file chunks

};
 

/* Using AHF by default */
void IOSettings::ReadHalos()
{
	string tmpStrUrlHalo;
	const char *tmpUrlHalo;
	const char *lineHead = "#";
	string lineIn;

	int nPartHalo = 0, nPTypes = 0, iTmpHalos = 0, nChunks = 0, nTmpHalos = 0; 

	nChunks = nChunksPerFile;

	// Initialize outside of the files loop
	int iLocHalos = 0; 
	
	for (int iChunk = 0; iChunk < nChunks; iChunk++)
	{
		tmpUrlHalo = haloFiles[iNumCat][iChunk].c_str();

		nTmpHalos = NumLines(tmpUrlHalo);	
		
		tmpParts.resize(nTmpHalos); 
		tmpHalos.resize(nTmpHalos); 

		iTmpHalos = 0;
		tmpPartsSize = 0;	// This will be increased while reading the file
		tmpHalosSize = sizeHalo * nTmpHalos;

		ifstream fileIn(tmpUrlHalo);
	
		if (!fileIn.good())
		{
			cout << "File: " << tmpUrlHalo << " not found on task=" << locTask << endl;
		} else { 
			if (locTask == 0)
	       			cout << "Reading " << nTmpHalos << " halos from file: " << tmpUrlHalo << endl;
		}

		while (getline(fileIn, lineIn))
		{
			const char *lineRead = lineIn.c_str();		
		
			if (lineRead[0] != lineHead[0])
			{
				tmpHalos[iTmpHalos].ReadLineAHF(lineRead);
				nPartHalo = tmpHalos[iTmpHalos].nPart[nPTypes];	// All particle types!
				tmpPartsSize += nPartHalo * sizePart;
				nTmpParts += nPartHalo; 

				// Assign halo to its nearest grid point - assign the absolute local index number
				GlobalGrid.AssignToGrid(tmpHalos[iTmpHalos].X, iLocHalos);

				iLocHalos++;
				iTmpHalos++;
			}
		}	// While Read Line

		fileIn.close();

		// Append to the locHalo file
		locHalos[iUseCat].insert(locHalos[iUseCat].end(), tmpHalos.begin(), tmpHalos.end());
		tmpHalos.clear();
		tmpHalos.shrink_to_fit();

	} // Loop on files per task

	//cout << "On task=" << locTask << " " << locPartsSize/1024/1024 << " MB pt and " << tmpHalosSize/1024/1024 << " MB hl " << endl; 
	//cout << "NHalos: " << tmpHalos.size() << " on task=" << locTask << endl;
};

   
void IOSettings::WriteTrees()
{

};

