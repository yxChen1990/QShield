#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>      /* vsnprintf */
#include <string.h>

#include <sgx_tcrypto.h>
#include <sgx_error.h>

#include "pbc/pbc.h"

#include "enclave.h"
#include "enclave_t.h"

/*
 * Following structs are defined for loaded data e.g., token, cipher, states
 */
#pragma pack(1)
typedef struct _token_t{
  uint8_t w;
  element_t skb;
} token_t;

typedef struct _s_id_t{
  bool is_used;
  char id[STATE_ID_MAX];
} s_id_t;
#pragma pack()

#define STATE_ID_NUM_MAX 20
s_id_t g_ids[STATE_ID_NUM_MAX] = {
  {false, "S1"},
  {false, "S2"},
  {false, "S3"},
  {false, "S4"},
  {false, "S5"},
  {false, "S6"},
  {false, "S7"},
  {false, "S8"},
  {false, "S9"},
  {false, "S10"},
  {false, "S11"},
  {false, "S12"},
  {false, "S13"},
  {false, "S14"},
  {false, "S15"},
  {false, "S16"},
  {false, "S17"},
  {false, "S18"},
  {false, "S19"},
  {false, "S20"},
};

char *get_g_id(){
  char *tmp = (char *)malloc(STATE_ID_MAX);
  int i = 0;
  for(; i < STATE_ID_NUM_MAX; i++){
    if(g_ids[i].is_used){
      continue;
    }else{
      g_ids[i].is_used = true;
      break;
    }
  }

  if(i == STATE_ID_NUM_MAX){
    return NULL;
  }
  strncpy(tmp, g_ids[i].id, STATE_ID_MAX);
  return tmp;
}

void eprintf(const char *fmt, ...){
  char buf[BUFSIZ] = {'\0'};
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, BUFSIZ, fmt, ap);
  va_end(ap);
  o_print_str(buf);
}

/*
 * enclave global variables
 */

pairing_t g_pairing; //for the bilinear map
errmsg_s_ptr g_errmsg;

void *g_rsa_pri_key; //for the enclave's private key
void *g_rsa_pub_key; //for the enclave's public key
sgx_ec256_private_t *g_ecdsa_sign_key; //for the enclave's signing key
sgx_ec256_public_t *g_ecdsa_verify_key; //for the enclave's verifing key

states_t *g_states[REQ_PARALLELISM]; //for all states within the enclave as per user

void e_states_init(){
  for(int i = 0; i < REQ_PARALLELISM; i++){
    g_states[i] = (states_t *)malloc(sizeof(states_t));
    g_states[i]->states_num = 0;
    g_states[i]->is_occupied = false;

    for(int j = 0; j < STATES_NUM_MAX; j++){
      g_states[i]->states[j].w = 0;
      memset(g_states[i]->states[j].s_id, 0, STATE_ID_MAX);
      memset(g_states[i]->states[j].f.func_name, 0, FUN_NAME_MAX);
      g_states[i]->states[j].p_states.p_sts_num = 0;
      for(int k = 0; k < PRE_STATES_NUM_MAX; k++){
        memset(g_states[i]->states[j].p_states.p_sts[k], 0, STATE_ID_MAX);
      }
      g_states[i]->states[j].s_db.coll_num = 0;
      for(int k = 0; k < STATE_COLLS_NUM_MAX; k++){
        g_states[i]->states[j].s_db.colls[k].docs_num = 0;
        memset(g_states[i]->states[j].s_db.colls[k].coll_id, 0, COLL_ID_MAX);
        for(int m = 0; m < COLL_DOCS_NUM_MAX; m++){
          g_states[i]->states[j].s_db.colls[k].docs[m].attrs_num = 0;
          for(int n = 0; n < DOC_ATTRS_NUM_MAX; n++){
            memset(g_states[i]->states[j].s_db.colls[k].docs[m].attrs[n].name, 0, ATTR_NAME_MAX);
            memset(g_states[i]->states[j].s_db.colls[k].docs[m].attrs[n].value, 0, ATTR_VALUE_MAX);
          }
        }
      }
    }
  }
}

sgx_status_t e_pairing_init(char* param, size_t count){
  sgx_status_t ret = SGX_SUCCESS;

  if (count < 0){
    eprintf("[Err]: parameter count error\n");
    ret = SGX_ERROR_UNEXPECTED;
    return ret;
  }

  // sgx_init_errmsg();
  if(pairing_init_set_buf(g_pairing, param, count)){
    eprintf("[Err]: pairing init failed\n");
    ret = SGX_ERROR_UNEXPECTED;
  }
  // g_errmsg = sgx_get_errmsg();
  // for(int i = 0; i < g_errmsg->err_num; i++){
  //   eprintf("[Pbc Debug %d]: %s\n",i, g_errmsg->errs[i].msg);
  // }
  // sgx_clear_errmsg();

  return ret;

}

sgx_status_t e_rsa_ecdsa_init(int n_byte_size, int e_byte_size){
  sgx_status_t ret = SGX_SUCCESS;

  unsigned char *p_e = (unsigned char *)malloc(e_byte_size);   // unsigned char p_e[4] = {1, 0, 1, 0};
  *p_e = '1';
  *(p_e+1) = '0';
  *(p_e+2) = '1';
  *(p_e+3) = '0';
  unsigned char *p_n = (unsigned char *)malloc(n_byte_size);
  unsigned char *p_d = (unsigned char *)malloc(n_byte_size);
  unsigned char *p_p = (unsigned char *)malloc(n_byte_size);
  unsigned char *p_q = (unsigned char *)malloc(n_byte_size);
  unsigned char *p_dmp1 = (unsigned char *)malloc(n_byte_size);
  unsigned char *p_dmq1 = (unsigned char *)malloc(n_byte_size);
  unsigned char *p_iqmp = (unsigned char *)malloc(n_byte_size);

  sgx_ecc_state_handle_t ecc_handle;
  g_ecdsa_sign_key= (sgx_ec256_private_t *)malloc(32);
  g_ecdsa_verify_key = (sgx_ec256_public_t *)malloc(32);

  ret = sgx_create_rsa_key_pair(n_byte_size, e_byte_size, p_n, p_d, p_e, p_p, p_q, p_dmp1, p_dmq1, p_iqmp);
  if(SGX_SUCCESS != ret){
    eprintf("[Err]: creating rsa key pair components failed\n");
    return ret;
  }
  ret = sgx_create_rsa_priv2_key(n_byte_size, e_byte_size, p_e, p_p, p_q, p_dmp1, p_dmq1, p_iqmp, &g_rsa_pri_key);
  if(SGX_SUCCESS != ret){
    eprintf("[Err]: creating rsa private key failed\n");
    return ret;
  }
  ret = sgx_create_rsa_pub1_key(n_byte_size, e_byte_size, p_n, p_e, &g_rsa_pub_key);
  if(SGX_SUCCESS != ret){
    eprintf("[Err]: creating rsa public key failed\n");
    return ret;
  }

  ret = sgx_ecc256_open_context(&ecc_handle);
  if(SGX_SUCCESS != ret){
    eprintf("[Err]: ecc256 context open failed\n");
    return ret;
  }
  ret = sgx_ecc256_create_key_pair(g_ecdsa_sign_key, g_ecdsa_verify_key, ecc_handle);
  if(SGX_SUCCESS != ret){
    eprintf("[Err]: creating signature key pair failed\n");
    return ret;
  }
  ret = sgx_ecc256_close_context(ecc_handle);
  if(SGX_SUCCESS != ret){
    eprintf("[Err]: ecc256 context close failed\n");
    return ret;
  }

  free(p_e);
  free(p_n);
  free(p_d);
  free(p_p);
  free(p_q);
  free(p_dmp1);
  free(p_dmq1);
  free(p_iqmp);
  return ret;
}

sgx_status_t e_decrypt(uint8_t* tk, size_t tk_size, uint8_t* ct, size_t ct_size, void* s_idx){
  sgx_status_t ret = SGX_SUCCESS;
  // recover token
  // token_t *p_tk = (token_t *)malloc(sizeof(token_t));
  // size_t p_tk_len;
  // ret = sgx_rsa_priv_decrypt_sha256(g_rsa_pri_key, &p_tk->w, &p_tk_len, tk, tk_size);
  // if(SGX_SUCCESS != ret){
  //   eprintf("[Err]: rsa decrypt token failed\n");
  //   return ret;
  // }

  // recover cipher text
  coll_db_t coll_db_tmp;


  // initialize state S0
  state_idx_t idx_tmp;
  // state_idx_t *idx_tmp = (state_idx_t *)malloc(sizeof(state_idx_t));
  idx_tmp.repo_id = 0;
  memset(idx_tmp.s_id, 0, STATE_ID_MAX);

  int i = 0;
  for(; i < REQ_PARALLELISM; i++){
    if(!g_states[i]->is_occupied){
      idx_tmp.repo_id = i;
      break;
    }
  }
  if(i == REQ_PARALLELISM){ //no available memory region for current request.
    ret = SGX_ERROR_OUT_OF_MEMORY;
    return ret;
  }
  g_states[idx_tmp.repo_id]->states_num = 1;
  g_states[idx_tmp.repo_id]->is_occupied = true;
  g_states[idx_tmp.repo_id]->states[0].w = 4; //test
  char id[] = "S0";
  strncpy(g_states[idx_tmp.repo_id]->states[0].s_id, id, sizeof(id));
  strncpy(g_states[idx_tmp.repo_id]->states[0].f.func_name, __FUNCTION__, sizeof(__FUNCTION__));
  memcpy(&g_states[idx_tmp.repo_id]->states[0].s_db.coll_num, &coll_db_tmp.coll_num, sizeof(coll_db_t));
  strncpy(idx_tmp.s_id, id, sizeof(id));
  memcpy((uint8_t *)s_idx, &idx_tmp.repo_id, sizeof(state_idx_t));

  return ret;
}

sgx_status_t e_projector(struct _pred_t p_pred, struct _state_idx_t s_in, void* s_out){

  if(p_pred.tp != 1 || p_pred.attrs_num == 0 || p_pred.colls_num != 1 \
      || s_in.repo_id >= REQ_PARALLELISM \
        || !g_states[s_in.repo_id]->is_occupied){
    return SGX_ERROR_INVALID_PARAMETER;
  }

  state_idx_t idx;
  {
    idx.repo_id = s_in.repo_id;
    char *id = get_g_id();
    if(NULL == id){
      return SGX_ERROR_UNEXPECTED;
    }
    memcpy(idx.s_id, id, STATE_ID_MAX);
    free(id);
  }



  memcpy((uint8_t *)s_out, &idx.repo_id, sizeof(state_idx_t));
  return SGX_SUCCESS;
}

sgx_status_t e_selector(struct _pred_t s_pred, struct _state_idx_t s_in, void* s_out){

  if(s_pred.tp != 2 || s_pred.attrs_num == 0 || s_pred.colls_num != 1 \
      || s_in.repo_id >= REQ_PARALLELISM \
        || !g_states[s_in.repo_id]->is_occupied){
    return SGX_ERROR_INVALID_PARAMETER;
  }

  state_idx_t idx;
  {
    idx.repo_id = s_in.repo_id;
    char *id = get_g_id();
    if(NULL == id){
      return SGX_ERROR_UNEXPECTED;
    }
    memcpy(idx.s_id, id, STATE_ID_MAX);
    free(id);
  }



  memcpy((uint8_t *)s_out, &idx.repo_id, sizeof(state_idx_t));
  return SGX_SUCCESS;
}

sgx_status_t e_aggregator(struct _pred_t a_pred, struct _state_idx_t s_in, void* s_out){

  if(a_pred.tp != 3 || a_pred.attrs_num == 0 || a_pred.colls_num != 1\
      || s_in.repo_id >= REQ_PARALLELISM \
        || !g_states[s_in.repo_id]->is_occupied){
    return SGX_ERROR_INVALID_PARAMETER;
  }

  state_idx_t idx;
  {
    idx.repo_id = s_in.repo_id;
    char *id = get_g_id();
    if(NULL == id){
      return SGX_ERROR_UNEXPECTED;
    }
    memcpy(idx.s_id, id, STATE_ID_MAX);
    free(id);
  }



  memcpy((uint8_t *)s_out, &idx.repo_id, sizeof(state_idx_t));
  return SGX_SUCCESS;
}

sgx_status_t e_joiner(struct _pred_t j_pred, struct _state_idx_t s_in_1, struct _state_idx_t s_in_2, void* s_out){

  if(j_pred.tp != 4 || j_pred.attrs_num == 0 || j_pred.colls_num != 2 \
      || s_in_1.repo_id != s_in_2.repo_id || s_in_1.repo_id >= REQ_PARALLELISM \
        || !g_states[s_in_1.repo_id]->is_occupied){
    return SGX_ERROR_INVALID_PARAMETER;
  }

  state_idx_t idx;
  {
    idx.repo_id = s_in_1.repo_id;
    char *id = get_g_id();
    if(NULL == id){
      return SGX_ERROR_UNEXPECTED;
    }
    memcpy(idx.s_id, id, STATE_ID_MAX);
    free(id);
  }



  memcpy((uint8_t *)s_out, &idx.repo_id, sizeof(state_idx_t));
  return SGX_SUCCESS;
}
