#pragma once

#include "TensorData.h"

#include <iostream>
#include <vector>

using namespace std;

class TensorDataSpAls : public TensorData
{
  public:
    TensorDataSpAls() : TensorData(), isSorted(false), isCmfReady(false){};
    TensorDataSpAls(const char *filename) : TensorData(filename), isSorted(false), isCmfReady(false){};

    void toFile(const char *filename) override;
    void fromFile(const char *filename) override;
    void printData(int did = -1);

    // T normData();
    unsigned verbose;

    void sortIndexes();

    vector<vector<size_t>> sortArgs;
    vector<T> dataCmf;

  protected:
    bool isSorted;
    bool isCmfReady;

    void sortIndex(const int notFrom, size_t *s);

    int findGreaterThan(const vector<double> &cmf, const int first, const int last, double p);

    void findIndexLoc(
	const vector<size_t> &froms,
	//const int from1, const int from2,
	const vector<size_t> &is,
	//, const int i2,
	const vector<size_t> &s,
	int &start,
	int &end);

    int findIndexLocBiSearchStart(
	const vector<size_t> &froms,
	const vector<size_t> &pivot,
	const vector<size_t> &s,
	int start,
	int end);

    inline bool EqThan(const vector<size_t> &rpivot, const vector<size_t> &lpivot, const vector<size_t> &froms)
    {
	for (int i = 0; i < froms.size(); i++)
	{
	    if (lpivot[froms[i]] == rpivot[froms[i]])
	    {
		continue;
	    }
	    else
	    {
		return false;
	    }
	}
	return true;
    }

    inline bool lessThan(const vector<size_t> &rpivot, const vector<size_t> &lpivot, const vector<size_t> &froms)
    {
	for (int i = 0; i < froms.size(); i++)
	{
	    if (lpivot[froms[i]] == rpivot[froms[i]])
	    {
		continue;
	    }
	    else
	    {
		return lpivot[froms[i]] < rpivot[froms[i]];
	    }
	}
	return false;
    }

    class myclass
    {
      public:
	// myclass(const vector<vector<size_t>> &_loc, const int _from1, const int _from2) :loc(_loc), from1(_from1), from2(_from2) {}
	// const int from1;
	// const int from2;

	myclass(const vector<vector<size_t>> &_loc, const vector<size_t> _froms) : loc(_loc), froms(_froms) {}
	const vector<size_t> froms;
	const vector<vector<size_t>> &loc;
	bool operator()(int i, int j)
	{
	    for (int did = 0; did < froms.size(); did++)
	    {
		int currFrom = froms[did];
		if (loc[j][currFrom] == loc[i][currFrom] && did + 1 < froms.size())
		{
		    continue;
		}
		else
		{
		    return loc[j][currFrom] > loc[i][currFrom];
		}
	    }
	}
    };
};