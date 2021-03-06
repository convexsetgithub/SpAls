#include "utils.h"
#include "TensorAls.h"
#include "CPDecomp.h"
#include "TensorData.h"

#include <memory>
#include <vector>
#include <chrono>
#include <cmath>

using namespace std;

TensorCP_SPALSOMP::TensorCP_SPALSOMP(const TensorDataSpAls &_data, shared_ptr<CPDecomp> &_cpd, SpAlsRNGeng *_rngEng, size_t _nthread)
    : TensorCP_SPALS(_data, _cpd, _rngEng), nthread(_nthread)
{
    // get balance;
    getBalanceDist();
    _rows = vector<vector<T>>(nthread, vector<T>(rank));
}

int TensorCP_SPALSOMP::updateFactorAls(const unsigned factorId)
{

    // log before iteration
    auto stepStartTime = logBeforIter(factorId);

    // clear the
    if (verbose > 2)
        cout << "Start clear factor and lambdas" << endl;
    cpd->lambdas = vector<T>(rank, 1.0);
    SpAlsUtils::reset(cpd->factors[factorId]);
    cpd->isFactorNormalized[factorId] = false;
    cpd->isGramUpdated[factorId] = false;
    cpd->isGramInvUpdated[factorId] = false;

    //get inverse gram matrix
    if (verbose > 2)
        cout << "Start prepareGramInv" << endl;
    prepareGramInv(factorId);

    if (verbose > 2)
        cout << "Start Getting froms" << endl;
    vector<size_t> froms = SpAlsUtils::getFroms(factorId, cpd->dims.size());
    if (verbose > 2)
        cout << "Start Updating entries" << endl;

#pragma omp parallel for
    for (int tid = 0; tid < nthread; ++tid)
    {
        const vector<size_t> &bl = bDistALS[factorId][tid];
        int i = -1;
        for (int k = 0, n = bl.size(); k < n; ++k)
        {
            i = bl[k + 1];
            updateEntry(factorId, froms, i, gramABpdtInv[factorId], 1.0, _rows[tid]);
        }
    }

    //postprocessing of the new factor
    if (verbose > 2)
    {
        cout << "print Factor Matrix Before Normalization:\n";
        SpAlsUtils::printMatrix(cpd->ro_factors[factorId]);
    }
    cpd->normalizeFactor(factorId);
    if (verbose > 2)
    {
        cout << "print Factor Matrix After Normalization:\n";
        SpAlsUtils::printMatrix(cpd->ro_factors[factorId]);
    }

    cpd->updateGram(factorId);
    if (verbose > 2)
    {
        cout << "\t\tgram matrix for factor after update:" << factorId << endl;
        SpAlsUtils::printMatrix(cpd->getGramMtx(factorId));
    }
    logAfterIter(factorId, stepStartTime);
    currIter++;
    return 0;
}

int TensorCP_SPALSOMP::updateFactor(const unsigned factorId, size_t count)
{
    // log before iteration
    auto stepStartTime = logBeforIter(factorId);

    // clear the
    cpd->lambdas = vector<T>(rank, 1.0);
    SpAlsUtils::reset(cpd->factors[factorId]);
    cpd->isFactorNormalized[factorId] = false;
    cpd->isGramUpdated[factorId] = false;
    cpd->isGramInvUpdated[factorId] = false;

    //get inverse gram matrix
    prepareGramInv(factorId);

    vector<size_t> froms = SpAlsUtils::getFroms(factorId, cpd->dims.size());
    size_t hitR = 0;
    size_t hitNZ = 0;

    const auto &sortArgs = dataSpals.sortArgs;
    const auto &loc = data.ro_loc;

    vector<vector<vector<int>>> taskes(nthread, vector<vector<int>>(nthread));
    vector<vector<vector<double>>> weights(nthread, vector<vector<double>>(nthread));

#pragma omp parallel for reduction(+ : hitR, hitNZ)
    for (int tid = 0; tid < nthread; ++tid)
    {
        vector<vector<int>> &taskT = taskes[tid];
        vector<vector<double>> &weightsT = weights[tid];

        for (int iter = (count / nthread + 1) * tid; iter < (count / nthread + 1) * (tid + 1) && iter < count; ++iter)
        {
            vector<size_t> ps;
            for (auto &fid : froms)
            {
                // SpAlsUtils::printVector(factorCmf[fid]);
                ps.push_back(SpAlsUtils::drawFromCmf(factorCmf[fid], rngEng[tid].nextRNG()));
                if (verbose > 3)
                    cout << fid << "\t" << ps.back() << endl;
            }

            double weight = 1.0 / count;
            for (size_t it = 0; it < froms.size(); it++)
            {
                if (ps[it] == 0)
                {
                    weight /= factorCmf[froms[it]][0];
                }
                else
                {
                    weight /= factorCmf[froms[it]][ps[it]] - factorCmf[froms[it]][ps[it] - 1];
                }
            }

            // get corresponding entries
            int start = -1;
            int end = -1;
            dataSpals.findEntryFromFactor(factorId, ps, start, end);

            if (start >= 0)
            {
                hitR++;
                hitNZ += end - start + 1;
                for (int i = start; i <= end; i++)
                {
                    int ttid = bDistSPALS[factorId][loc[sortArgs[factorId][i]][factorId]];
                    taskes[tid][ttid].push_back(sortArgs[factorId][i]);
                    weights[tid][ttid].push_back(weight);
                }
            }
        }
    }

#pragma omp parallel for
    for (int ttid = 0; ttid < nthread; ++ttid)
    {
        for (int tid = 0; tid < nthread; ++tid)
        {
            for (int i = 0, ns = taskes[tid][ttid].size(); i < ns; i++)
            {
                int p = taskes[tid][ttid][i];
                updateEntry(factorId, froms, p, gramABpdtInv[factorId], weights[tid][ttid][i], _rows[ttid]);
            }
        }
    }

    if (verbose)
        cout << "Sample NNZ Rows:" << hitR << "\tSample NNZ Entries:" << hitNZ << "\tSample NNZ Rows:" << count << endl;
    //postprocessing of the new factor
    if (verbose > 2)
    {
        cout << "print Factor Matrix Before Normalization:\n";
        SpAlsUtils::printMatrix(cpd->ro_factors[factorId]);
    }
    cpd->normalizeFactor(factorId);
    if (verbose > 2)
    {
        cout << "print Factor Matrix After Normalization:\n";
        SpAlsUtils::printMatrix(cpd->ro_factors[factorId]);
    }

    cpd->updateGram(factorId);
    if (verbose > 2)
    {
        cout << "\t\tgram matrix for factor after update:" << factorId << endl;
        SpAlsUtils::printMatrix(cpd->getGramMtx(factorId));
    }

    logAfterIter(factorId, stepStartTime);
    currIter++;
    return 0;
}

int TensorCP_SPALSOMP::updateFactor(const unsigned factorId)
{
    if (rate <= 0)
        return updateFactorAls(factorId);

    size_t count = pow(rank, data.ro_dims.size() - 1) * rate;
    for (int fid = 0; fid < data.ro_dims.size(); fid++)
    {
        if (fid != factorId)
        {
            count *= max(1, (int)(log(data.ro_dims[fid])));
        }
    }
    return updateFactor(factorId, count);
}

void TensorCP_SPALSOMP::getBalanceDist()
{
    auto NDIM = data.ro_dims.size();
    const auto &dims = data.ro_dims;

    bDistALS = vector<vector<vector<size_t>>>(NDIM, vector<vector<size_t>>(nthread, vector<size_t>()));
    bDistSPALS = vector<vector<size_t>>(NDIM);

    for (size_t i = 0; i < NDIM; i++)
    {
        bDistSPALS[i] = vector<size_t>(dims[i]);
    }

    // get count for each row
    const auto nnz = data.ro_nnz;
    const auto &loc = data.ro_loc;
#pragma omp parallel for
    for (int did = 0; did < NDIM; did++)
    {
        vector<unsigned> rcount(dims[did]);
        for (size_t i = 0; i < nnz; ++i)
        {
            ++rcount[loc[i][did]];
        }

        int thd = (nnz / nthread) + 1;
        int currentLoad = 0;
        size_t tid = 0;
        for (size_t i = 0; i < dims[did]; ++i)
        {
            bDistSPALS[did][i] = tid;
            currentLoad += rcount[i];
            if (currentLoad >= thd)
            {
                tid++;
                currentLoad = 0;
            }
        }
        tid++;

        for (size_t i = 0; i < nnz; ++i)
        {
            bDistALS[did][bDistSPALS[did][loc[i][did]]].push_back(i);
        }
    }
}
