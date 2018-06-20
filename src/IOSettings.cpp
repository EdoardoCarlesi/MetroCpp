#include <string>
#include <sstream>
#include <fstream>

#include "IOSettings.h"
#include "general.h"

using namespace std;


IOSettings::IOSettings() 
{
};


IOSettings::~IOSettings() 
{
};


void IOSettings::Init()
{
	// Initialize all the main variables, checking for consistency
};


void IOSettings::DistributeFilesAmongTasks(void)
{
};


// Particle sizes have already been allocated in the ReadHalos() routines, do a safety check for the size
void IOSettings::ReadParticles(void)
{
	string strUrlPart = pathInput + urlTestFilePart;	// FIXME : this is only a test url for the moment
	const char *urlPart = strUrlPart.c_str();
	unsigned int iLocHalos = 0, iLocParts = 0, totLocParts = 0, iLine = 0, nPartHalo = 0, nFileHalos = 0;		
	unsigned long long int locHaloID;
	string lineIn;

	ifstream fileIn(urlPart);
		
	if (!fileIn.good())
		cout << "File: " << urlPart << " not found on task=" << locTask << endl;
	else 
	        cout << "Task=" << locTask << " is reading " << nLocParts << " halos from file: " << urlPart << endl;
	
	while (getline(fileIn, lineIn))
	{
		const char *lineRead = lineIn.c_str();		

		if (iLine == 0)
		{
	                sscanf(lineRead, "%d", &nFileHalos);
	
			if (nFileHalos != nLocHalos && iLine == 0)
			{
				cout << "WARNING on task=" << locTask << " expected: " << nLocHalos << " found: " <<  nFileHalos << endl;
				iLine++;
			} else {
				cout << "Particle file contains " << nLocHalos << " halos." << endl; 
				iLocHalos++;
				iLine++;
			}

		} else if (iLine == 1) {

	                sscanf(lineRead, "%u %llu", &nPartHalo, &locHaloID);
			iLine++;

		} else {

			locParts[iLocHalos-1][iLocParts].ReadLineAHF(lineRead);
			totLocParts++;
			iLocParts++;

			if (iLocParts == locHalos[iLocHalos-1].nPart)
			{
				locHalos[iLocHalos-1].Part = locParts[iLocHalos-1];
				iLine = 1;	// Reset some indicators
				iLocParts = 0;
			}
		} // else iLine not 0 or 1
	} // end while

};
 

/* Using AHF by default */
void IOSettings::ReadHalos()
{
	string strUrlHalo = pathInput + urlTestFileHalo;	// FIXME : this is only a test url for the moment
	const char *urlHalo = strUrlHalo.c_str();
	const char *lineHead = "#";
	string lineIn;

	size_t haloSize = sizeof(Halo), partSize = sizeof(Particle);
	unsigned int iLocHalos = 0;
	
	if (locTask == 0)
		nLocHalos = NumLines(urlHalo) - nLinesHeader;	
	else
		nLocHalos = NumLines(urlHalo);	
	
	locHalos = new Halo[nLocHalos];
	locParts = (Particle **) new Particle[nLocHalos];

	locPartsSize = 0;	// This will be increased while reading the file
	locHalosSize = haloSize * nLocHalos;

	ifstream fileIn(urlHalo);
	
	if (!fileIn.good())
		cout << "File: " << urlHalo << " not found on task=" << locTask << endl;
	else 
	        cout << "Task=" << locTask << " is reading " << nLocHalos << " halos from file: " << urlHalo << endl;

	while (getline(fileIn, lineIn))
	{
		const char *lineRead = lineIn.c_str();		
	
		if (lineRead[0] != lineHead[0])
		{
			locHalos[iLocHalos].ReadLineAHF(lineRead);
			locParts[iLocHalos] = new Particle[locHalos[iLocHalos].nPart];
			locPartsSize += locHalos[iLocHalos].nPart * partSize;
			iLocHalos++;
		}
	}
	
	cout << "On task=" << locTask << " " << locPartsSize/1024/1024 << " MB pt and " << locHalosSize/1024/1024 << " MB hl " << endl; 
	locHalos[0].Info();
	locHalos[10].Info();
};

   
void IOSettings::WriteTrees()
{

};

