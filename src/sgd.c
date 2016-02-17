
#include "sgd.h"
#include "reorder.h"
#include "util.h"



static val_t p_calc_obj(
    sptensor_t const * const train,
    splatt_kruskal const * const model,
    val_t * const buffer,
    val_t const * const regularization)
{
  idx_t const nfactors = model->rank;
  idx_t const nmodes = train->nmodes;

  val_t reg_obj = 0.;
  val_t loss_obj = 0.;
  #pragma omp parallel reduction(+:reg_obj,loss_obj)
  {
    for(idx_t m=0; m < nmodes; ++m) {
      val_t accum = 0;
      val_t const * const restrict mat = model->factors[m];
      #pragma omp for schedule(static) nowait
      for(idx_t x=0; x < model->dims[m] * nfactors; ++x) {
        accum += mat[x] * mat[x];
      }

      reg_obj += regularization[m] * accum;
    }

    val_t const * const restrict train_vals = train->vals;
    #pragma omp for schedule(static) nowait
    for(idx_t x=0; x < train->nnz; ++x) {
      val_t const err = train_vals[x] - predict_val(model, train, x, buffer);
      loss_obj += err * err;
    }
  }

  return loss_obj + reg_obj;
}


static void p_update_model(
    sptensor_t const * const train,
    idx_t const nnz_index,
    splatt_kruskal const * const model,
    val_t * const buffer,
    val_t const learn_rate,
    val_t const * const regularization,
    val_t const err)
{
  idx_t const nfactors = model->rank;
  idx_t const nmodes = train->nmodes;

  for(idx_t m=0; m < nmodes; ++m) {
    for(idx_t f=0; f < nfactors; ++f) {
      buffer[f] = 1.;
    }

    for(idx_t m2=0; m2 < nmodes; ++m2) {
      if(m2 != m) {
        val_t const * const restrict row = model->factors[m2] +
            (train->ind[m2][nnz_index] * nfactors);
        for(idx_t f=0; f < nfactors; ++f) {
          buffer[f] *= row[f];
        }
      }
    }

    val_t * const restrict update_row = model->factors[m] +
        (train->ind[m][nnz_index] * nfactors);
    val_t const reg = regularization[m];
    for(idx_t f=0; f < nfactors; ++f) {
      update_row[f] += learn_rate * ((err * buffer[f]) - (reg * update_row[f]));
    }
  }
}


void splatt_sgd(
    sptensor_t const * const train,
    sptensor_t const * const validate,
    splatt_kruskal * const model,
    idx_t const max_epochs,
    val_t learn_rate,
    val_t const * const regularization)
{
  idx_t const nfactors = model->rank;
  val_t const * const restrict train_vals = train->vals;

  idx_t * perm = splatt_malloc(train->nnz * sizeof(*perm));

  val_t * predict_buffer = splatt_malloc(nfactors * sizeof(*predict_buffer));
  val_t * all_others = splatt_malloc(nfactors * sizeof(*all_others));

  /* ensure lambda=1 */
  for(idx_t f=0; f < nfactors; ++f) {
    model->lambda[f] = 1.;
  }

  /* init perm */
  for(idx_t n=0; n < train->nnz; ++n) {
    perm[n] = n;
  }

  val_t prev_obj = 0;

  /* foreach epoch */
  for(idx_t e=0; e < max_epochs; ++e) {
    /* new nnz ordering */
    shuffle_idx(perm, train->nnz);

    for(idx_t n=0; n < train->nnz; ++n) {
      idx_t const x = perm[n];

      val_t const err = train_vals[x] - predict_val(model, train, x,
          predict_buffer);
      p_update_model(train, x, model, all_others, learn_rate, regularization, err);
    }

    /* compute RMSE and adjust learning rate */

    val_t const obj = p_calc_obj(train, model, predict_buffer, regularization);
    val_t const train_rmse = kruskal_rmse(train, model);
    val_t const val_rmse = kruskal_rmse(validate, model);
    printf("epoch=%"SPLATT_PF_IDX"   obj=%0.5e   tr-rmse=%0.5e   v-rmse=%0.5e\n",
        e+1, obj, train_rmse, val_rmse);

    if(e > 0) {
      if(obj < prev_obj) {
        //learn_rate *= 1.02;
      } else {
        //learn_rate *= 1.50;
      }
    }

    prev_obj = obj;
  }

  splatt_free(predict_buffer);
  splatt_free(all_others);
  splatt_free(perm);
}


