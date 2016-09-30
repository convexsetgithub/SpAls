#include "utils.h"
#include "TensorAls.h"
#include "CPDecomp.h"
#include "TensorData.h"
#include "SpAlsLinalg.h"

#include <memory>
#include <vector>
#include <chrono>
#include <cmath>

using namespace std;

TensorCP_SPALS::TensorCP_SPALS(const TensorDataSpAls &_data, shared_ptr<CPDecomp> &_cpd, SpAlsRNGeng &_rngEng)
    : TensorCP_ALS(_data, _cpd), rngEng(_rngEng), rate(1.0)
{
    lvrgScores = vector<vector<T>>(data.ro_dims.size());
    factorCmf = vector<vector<double>>(data.ro_dims.size());
    for (size_t factorId = 0; factorId < data.ro_dims.size(); factorId++)
    {
        lvrgScores[factorId] = vector<T>(data.ro_dims[factorId]);
        factorCmf[factorId] = vector<T>(data.ro_dims[factorId]);
    }
}
int TensorCP_SPALS::updateFactor(const unsigned factorId, size_t count)
{
    return 0;
}

int TensorCP_SPALS::updateFactor(const unsigned factorId)
{

    size_t count = pow(rank, data.ro_dims.size() - 1) * rate;
    for (auto &fid : data.ro_dims)
    {
        if (fid != factorId)
        {
            count *= max(1, (int)(log(data.ro_dims[fid])));
        }
    }
    return updateFactor(factorId, count);
}

void TensorCP_SPALS::getLvrgScr(const unsigned factorId)
{
    cpd->updateGram(factorId);

    size_t nid = data.ro_dims[factorId];
    SpAlsUtils::reset(lvrgScores[factorId]);

    auto &factor = cpd->factors[factorId];
    auto &gramMtxInv = cpd->getGramMtxInv(factorId);

#pragma omp parallel for
    for (int i = 0; i < nid; ++i)
    {
        for (size_t j1 = 0; j1 < rank; j1++)
        {
            for (size_t j2 = 0; j2 < rank; j2++)
            {
                lvrgScores[factorId][i] += factor[i][j1] * gramMtxInv[j1][j2] * factor[i][j2];
            }
        }
    }
    SpAlsUtils::pdf2Cmf(lvrgScores[factorId], factorCmf[factorId]);
}

void TensorCP_SPALS::setRate(double _rate)
{
    rate = _rate;
}