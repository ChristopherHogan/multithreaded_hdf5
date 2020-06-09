// FUNC_ENTER_API
hbool_t err_occurred = 0;
static hbool_t func_check = 0;
if(!func_check) {
  assert(is_api_call);
  func_check = 1;
}

pthread_once(&H5TS_first_init_g, H5TS_pthread_first_thread_init);
H5TS_cancel_count_inc();
H5TS_mutex_lock(&H5_g.init_lock);
if(!(H5_g.H5_libinit_g) && !(H5_g.H5_libterm_g)) {
  (H5_g.H5_libinit_g) = 1;
  if(H5_init_library() < 0) {
    // TODO(chogan): Global macros
    H5E_printf_stack(0, "fname", __func__, 166, H5E_ERR_CLS_g, ( H5E_FUNC_g), ( H5E_CANTINIT_g), "library initialization failed");
    err_occurred = 1;
    err_occurred = err_occurred;
    ret_value = -1;
    goto done;
  }
}
H5_PACKAGE_YES_INIT((-1))
if(H5CX_push() < 0) {
  H5E_printf_stack(((void *)0), "/home/chogan/dev/hdf5/src/H5Dio.c", __func__, 166, H5E_ERR_CLS_g, ( H5E_FUNC_g), ( H5E_CANTSET_g), "can't set API context");
  err_occurred = 1;
  err_occurred = err_occurred;
  {
    ret_value = (-1);
    goto done;
  }
}
H5E_clear_stack(((void *)0));
// FUNC_LEAVE_API
(void)H5CX_pop();
if(err_occurred) {
  (void)H5E_dump_api_stack(1);
}
H5TS_mutex_unlock(&H5_g.init_lock);
H5TS_cancel_count_dec();
return(ret_value);

// FUNC_ENTER_NOAPI
hbool_t err_occurred = 0;
static hbool_t func_check = 0;
if(!func_check) {
  assert(not_an_api_func);
  func_check = 1;
}
if((1) || !(H5_g.H5_libterm_g)) {
  // FUNC_LEAVE_NOAPI
  ;
}
return(ret_value);


// H5_PACKAGE_INIT(H5D, FAIL)
if(!H5D_init_g && !(H5_g.H5_libterm_g)) {
  H5D_init_g = 1;
  if(H5D__init_package() < 0) {
    H5D_init_g = 0;
    H5E_printf_stack(((void *)0), "/home/chogan/dev/hdf5/src/H5Dio.c", __func__, 163, H5E_ERR_CLS_g, ( H5E_FUNC_g), ( H5E_CANTINIT_g), "interface initialization failed");
    err_occurred = 1;
    err_occurred = err_occurred;;
    ret_value = (-1);
    goto done;
  }
}

// H5SL_SEARCH(SCALAR, slist, x, const hid_t, key, -)
if((slist)->safe_iterating) {
  int _i;
  H5SL_node_t *_low = x;
  H5SL_node_t *_high = ((void *)0);
  H5SL_LOCATE_SCALAR_HASHINIT(key, -);
  for(_i = (int)slist->curr_level; _i >= 0; _i--) {
    x = _low->forward[_i];
    while(x != _high) {
      if(!x->removed) {
        if(H5SL_LOCATE_SCALAR_CMP(slist, const hid_t, x, key, -))
          _low = x;
        else
          break;
      }
      x = x->forward[_i];
    }
    _high = x;
    if(x != ((void *)0) && H5SL_LOCATE_SCALAR_EQ(slist, const hid_t, x, key, -) ) {
      H5SL_LOCATE_SEARCH_FOUND(slist, x, _i);
      break;
    }
  }
} else {
  int _i;
  unsigned _count;
  H5SL_LOCATE_SCALAR_HASHINIT(key, -);
  for(_i = (int)slist->curr_level; _i >= 0; _i--) {
    _count = 0;
    while(_count < 3 && x->forward[_i] && (*(const hid_t *)((x->forward[_i])->key) < *(const hid_t *)key) ) {
      x = x->forward[_i];
      _count++;
    }
  }
  x = x->forward[0];
  if(x != ((void *)0) && (*(const hid_t *)((x)->key) == *(const hid_t *)key) ) {
    assert(!x->removed);
    ret_value = x->item;
    goto done;
  }
}

// FUNC_ENTER_PACKAGE_TAG(dataset->oloc.addr)
haddr_t prev_tag = ((haddr_t)(long)(-1));
hbool_t err_occurred = 0;
static hbool_t func_check = 0;
  if(!func_check) {
    assert(is_package_func);
    func_check = 1;
  }
H5AC_tag(dataset->oloc.addr, &prev_tag);
if(H5D_init_g || !(H5_g.H5_libterm_g)) {
