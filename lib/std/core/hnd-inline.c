

/*---------------------------------------------------------------------------
  Copyright 2020 Microsoft Corporation.

  This is free software; you can redistribute it and/or modify it under the
  terms of the Apache License, Version 2.0. A copy of the License can be
  found in the file "license.txt" at the root of this distribution.
---------------------------------------------------------------------------*/
/*
typedef datatype_t __std_core_hnd__ev;
struct __std_core_hnd_Ev {
  __std_core_hnd__htag _field1;
  box_t _field3;
  __std_core_hnd__evv _field4;
  __std_core_hnd__marker _field2;
};
*/

static inline struct __std_core_hnd_Ev* unbox_ev(box_t b, context_t* ctx) {
  return __std_core_hnd__as_Ev(unbox___std_core_hnd__ev(b,ctx));
}

static inline box_t* evv_as_vec(evv_t evv, size_t* len, box_t* single) {  
  if (evv_is_vector(evv)) {
    return vector_buf(evv,len);
  }
  else {
    // single evidence
    *single = box_datatype(evv);
    *len = 1;
    return single;
  }
}

struct __std_core_hnd__ev_s* ev_none(context_t* ctx) {
  static __std_core_hnd__ev ev_none_singleton;
  if (ev_none_singleton==NULL) {
    ev_none_singleton = __std_core_hnd__new_Ev(
      reuse_null,
      __std_core_hnd__new_Htag(dup_string_t(string_empty),ctx), // tag ""
      __std_core_hnd__new_Marker(-1,ctx),                       // marker -1
      box_null,                                                 // no handler
      vector_empty(),
      ctx
    );      
  }
  return dup___std_core_hnd__ev(ev_none_singleton);
}


int32_t evv_index( struct __std_core_hnd_Htag htag, context_t* ctx ) {
  // todo: drop htag?
  size_t len;
  box_t single;
  box_t* vec = evv_as_vec(ctx->evv,&len,&single);
  for(size_t i = 0; i < len; i++) {
    struct __std_core_hnd_Ev* ev = unbox_ev(vec[i],ctx);
    if (string_cmp_borrow(htag._field1,ev->_field1._field1) <= 0) return (int32_t)(i); // break on insertion point
  }
  //string_t evvs = evv_show(dup_datatype_as(evv_t,ctx->evv),ctx);
  //fatal_error(EFAULT,"cannot find tag '%s' in: %s", string_cbuf_borrow(htag._field1), string_cbuf_borrow(evvs));
  //drop_string_t(evvs,ctx);  
  return (int32_t)len;
}

__std_core_hnd__ev evv_lookup( struct __std_core_hnd_Htag htag, context_t* ctx ) {
  // todo: drop htag
  int32_t idx = evv_index(htag,ctx);
  return evv_at(idx,ctx);
}


evv_t evv_insert(evv_t evvd, __std_core_hnd__ev evd, context_t* ctx) {
  struct __std_core_hnd_Ev* ev = __std_core_hnd__as_Ev(evd);
  // update ev
  int32_t marker = ev->_field2.m;
  if (marker < 0) { return evvd; } // ev-none 
  drop_evv_t(ev->_field4,ctx);
  ev->_field4 = evvd;     // dup evvd
  if (marker==0) { return dup_evv_t(evvd); } // zero marker means this evidence is not in the evidence vector
  // insert ev
  size_t n;
  box_t single;
  const box_t* evv1 = evv_as_vec(evvd, &n, &single);
  const vector_t vec2 = vector_alloc(n+1,box_null,ctx);
  box_t* const evv2 = vector_buf(vec2,NULL);
  size_t i;
  for(i = 0; i < n; i++) {
    box_t evb1 = evv1[i];
    const struct __std_core_hnd_Ev* ev1 = unbox_ev(evb1,ctx);
    if (string_cmp_borrow(ev->_field1._field1,ev1->_field1._field1) <= 0) break;
    evv2[i] = dup_box_t(evb1); // use dup_datatype for efficiency?
  }
  evv2[i] = box_basetype(evd);
  for(; i < n; i++) {
    evv2[i+1] = dup_box_t(evv1[i]);  // use dup_datatype for efficiency?
  }
  // drop_datatype(evvd,ctx);  // assigned to evidence already
  return vec2;
}

evv_t evv_delete(evv_t evvd, int32_t index, bool behind, context_t* ctx) {
  size_t n;
  box_t single;
  const box_t* evv1 = evv_as_vec(evvd, &n, &single);
  if (n <= 1) {
    drop_datatype(evvd,ctx);
    return evv_total(ctx);
  }
  if (behind) index++;
  assert_internal((size_t)index < n);  
  const vector_t vec2 = vector_alloc(n+1,box_null,ctx);  
  box_t* const evv2 = vector_buf(vec2,NULL);
  size_t i;
  for(i = 0; i < (size_t)index; i++) {
    evv2[i] = dup_box_t(evv1[i]);  // todo: use box_datatype for efficiency?
  }
  for(; i < n-1; i++) {
    evv2[i] = dup_box_t(evv1[i+1]);
  }
  drop_datatype(evvd,ctx);
  return vec2;
}

string_t evv_show(evv_t evv, context_t* ctx) {
  return string_alloc_dup("(not yet implemented: evv_show)",ctx);
}



/*-----------------------------------------------------------------------
  Compose continuations
-----------------------------------------------------------------------*/

struct kcompose_fun_s {
  struct function_s _base;  
  box_t      count;
  function_t conts[1];
};

// kleisli composition of continuations
static box_t kcompose( function_t fself, box_t x, context_t* ctx) {
  struct kcompose_fun_s* self = function_as(struct kcompose_fun_s*,fself);
  intx_t count = unbox_int(self->count);
  function_t* conts = &self->conts[0];  
  // call each continuation in order
  for(intx_t i = 0; i < count; i++) {
    // todo: take uniqueness of fself into account to avoid dup_function
    function_t f = dup_function_t(conts[i]);
    x = function_call(box_t, (function_t, box_t, context_t*), f, (f, x, ctx));
    if (yielding(ctx)) {
      // if yielding, `yield_next` all continuations that still need to be done
      while(++i < count) {
        // todo: if fself is unique, we could copy without dup?
        yield_extend(dup_function_t(conts[i]),ctx);        
      }
      drop_function_t(fself,ctx);
      drop_box_t(x,ctx);
      return box_any(ctx); // return yielding
    }
  }
  drop_function_t(fself,ctx);
  return x;
}

static function_t new_kcompose( function_t* conts, intx_t count, context_t* ctx ) {
  if (count<=0) return function_id(ctx);
  if (count==1) return conts[0];
  struct kcompose_fun_s* f = block_as(struct kcompose_fun_s*, 
                               block_alloc(sizeof(struct kcompose_fun_s) - sizeof(function_t) + (count*sizeof(function_t)), 
                                 2 + count /* scan size */, TAG_FUNCTION, ctx));
  f->_base.fun = box_cptr(&kcompose,ctx);
  f->count = box_int(count);
  memcpy(f->conts, conts, count * sizeof(function_t));
  return (&f->_base);                              
}

/*-----------------------------------------------------------------------
  Yield extension
-----------------------------------------------------------------------*/

box_t yield_extend( function_t next, context_t* ctx ) {
  yield_t* yield = &ctx->yield;
  assert_internal(yielding(ctx));  // cannot extend if not yielding
  if (unlikely(yielding_final(ctx))) {
    // todo: can we optimize this so `next` is never allocated in the first place?
    drop_function_t(next,ctx); // ignore extension if never resuming
  }
  else {
    if (unlikely(yield->conts_count >= YIELD_CONT_MAX)) {
      // alloc a function to compose all continuations in the array
      function_t comp = new_kcompose( yield->conts, yield->conts_count, ctx );
      yield->conts[0] = comp;
      yield->conts_count = 1;
    }
    yield->conts[yield->conts_count++] = next;
  }
  return box_any(ctx);
}

// cont_apply: \x -> f(cont,x) 
struct cont_apply_fun_s {
  struct function_s _base;
  function_t f;
  function_t cont;
};

static box_t cont_apply( function_t fself, box_t x, context_t* ctx ) {
  struct cont_apply_fun_s* self = function_as(struct cont_apply_fun_s*, fself);
  function_t f = self->f;
  function_t cont = self->cont;  
  drop_match(self,{dup_function_t(f);dup_function_t(cont);},{},ctx);
  return function_call( box_t, (function_t, function_t, box_t, context_t* ctx), f, (f, cont, x, ctx));  
}

function_t new_cont_apply( function_t f, function_t cont, context_t* ctx ) {
  struct cont_apply_fun_s* self = function_alloc_as(struct cont_apply_fun_s, 2, ctx);
  self->_base.fun = box_cptr(&cont_apply,ctx);
  self->f = f;
  self->cont = cont;
  return (&self->_base);
}

// Unlike `yield_extend`, `yield_cont` gets access to the current continuation. This is used in `yield_prompt`.
box_t yield_cont( function_t f, context_t* ctx ) {
  yield_t* yield = &ctx->yield;
  assert_internal(yielding(ctx)); // cannot extend if not yielding
  if (unlikely(yielding_final(ctx))) {
    drop_function_t(f,ctx); // ignore extension if never resuming
  }
  else {
    function_t cont = new_kcompose(yield->conts, yield->conts_count, ctx);
    yield->conts_count = 1;
    yield->conts[0] = new_cont_apply(f, cont, ctx);
  }
  return box_any(ctx);
}

function_t yield_to( struct __std_core_hnd_Marker m, function_t clause, context_t* ctx ) {
  yield_t* yield = &ctx->yield;
  assert_internal(!yielding(ctx)); // already yielding  
  ctx->yielding = YIELD_NORMAL;
  yield->marker = m.m;
  yield->clause = clause;
  yield->conts_count = 0;
  return unbox_basetype_as(function_t,box_any(ctx));
}

box_t yield_final( struct __std_core_hnd_Marker m, function_t clause, context_t* ctx ) {
  yield_to(m,clause,ctx);
  ctx->yielding = YIELD_FINAL;
  return box_any(ctx);
}

box_t fatal_resume_final(context_t* ctx) {
  fatal_error(EFAULT,"trying to resume a finalized resumption");
  return box_any(ctx);
}

static box_t _fatal_resume_final(function_t self, context_t* ctx) {
  drop_function_t(self,ctx);
  return fatal_resume_final(ctx);
}
static function_t fun_fatal_resume_final(context_t* ctx) {
  define_static_function(f,_fatal_resume_final,ctx);
  return dup_function_t(f);
}


struct __std_core_hnd_yld_s  yield_prompt( struct __std_core_hnd_Marker m, context_t* ctx ) {
  yield_t* yield = &ctx->yield;
  if (ctx->yielding == YIELD_NONE) {
    return __std_core_hnd__new_Pure(ctx);
  }
  else if (yield->marker != m.m) {
    return (ctx->yielding == YIELD_FINAL ? __std_core_hnd__new_YieldingFinal(ctx) : __std_core_hnd__new_Yielding(ctx));
  }
  else {
    function_t cont = (ctx->yielding == YIELD_FINAL ? fun_fatal_resume_final(ctx) : new_kcompose(yield->conts, yield->conts_count, ctx));
    function_t clause = yield->clause;
    ctx->yielding = YIELD_NONE;
    #ifndef NDEBUG
    memset(yield,0,sizeof(yield_t));
    #endif
    return __std_core_hnd__new_Yield(clause, cont, ctx);
  }
}

unit_t  evv_guard(evv_t evv, context_t* ctx) {
  if (!datatype_eq(ctx->evv,evv)) {
    // todo: improve error message with diagnostics
    fatal_error(EFAULT,"trying to resume outside the (handler) scope of the original handler");
  }
  return Unit;
}


evv_t evv_create(evv_t evv, vector_t indices, context_t* ctx) {
  unsupported_external("evv_create");
  return vector_empty();
}

evv_t evv_swap_create( vector_t indices, context_t* ctx ) {
  size_t len;
  box_t* vec = vector_buf(indices,&len);
  if (len==0) {
    drop_vector_t(indices,ctx);
    return evv_swap_create0(ctx);
  }
  if (len==1) {
    int32_t i = unbox_int32_t(vec[0],ctx);
    drop_vector_t(indices,ctx);
    return evv_swap_create1(i,ctx);
  }
  return evv_swap( evv_create(dup_evv_t(ctx->evv),indices,ctx), ctx );
}

typedef struct yield_info_s {
  struct __std_core_hnd__yield_info_s _base;
  function_t clause;          
  function_t conts[YIELD_CONT_MAX];
  size_t     conts_count;     
  int32_t    marker;          
  uint8_t    yielding;
}* yield_info_t;

__std_core_hnd__yield_info yield_capture(context_t* ctx) {  
  assert_internal(yielding(ctx));
  yield_info_t yld = block_alloc_as(struct yield_info_s, 1 + YIELD_CONT_MAX, (tag_t)1, ctx);
  yld->clause = ctx->yield.clause; 
  size_t i = 0;
  for( ; i < ctx->yield.conts_count; i++) {
    yld->conts[i] = ctx->yield.conts[i];
  }
  for( ; i < YIELD_CONT_MAX; i++) {
    yld->conts[i] = function_null(ctx);
  }
  yld->conts_count = ctx->yield.conts_count;
  yld->marker = ctx->yield.marker;
  yld->yielding = ctx->yielding;
  ctx->yielding = 0;
  ctx->yield.conts_count = 0;
  return datatype_from_base(&yld->_base);
}

box_t yield_reyield( __std_core_hnd__yield_info yldinfo, context_t* ctx) {
  assert_internal(!yielding(ctx));
  yield_info_t yld = datatype_as_assert(yield_info_t, yldinfo, (tag_t)1);  
  ctx->yield.clause = dup_function_t(yld->clause);
  ctx->yield.marker = yld->marker;
  ctx->yield.conts_count = yld->conts_count;
  ctx->yielding = yld->yielding;
  for(size_t i = 0; i < yld->conts_count; i++) {
    ctx->yield.conts[i] = dup_function_t(yld->conts[i]);
  }
  drop_constructor(yld,ctx);
  return box_any(ctx);
}
