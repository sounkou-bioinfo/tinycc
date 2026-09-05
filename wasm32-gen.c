#ifdef TARGET_DEFS_ONLY

#define NB_REGS 16

#define RC_INT   0x00000001
#define RC_FLOAT 0x00000002
#define RC_IRET  0x00000004
#define RC_IRE2  0x00000008
#define RC_FRET  0x00000010

#define REG_IRET 0
#define REG_IRE2 1
#define REG_FRET 8

#define PTR_SIZE 4
#define LDOUBLE_SIZE 8
#define LDOUBLE_ALIGN 8
#define MAX_ALIGN 16
#define CHAR_IS_UNSIGNED
#define FUNC_STRUCT_PARAM_AS_PTR
#define TCC_USING_DOUBLE_FOR_LDOUBLE 1

#else

#define USING_GLOBALS
#include "tcc.h"
#include "wasm32-twir.h"

ST_DATA const char * const target_machine_defs =
    "__wasm32__\0"
    "__wasm32\0"
    "__EMSCRIPTEN__\0"
    "__SIZEOF_POINTER__ 4\0"
    "__BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__\0"
    ;

ST_DATA const int reg_classes[NB_REGS] = {
    RC_INT | RC_IRET,
    RC_INT | RC_IRE2,
    RC_INT,
    RC_INT,
    RC_INT,
    RC_INT,
    RC_INT,
    RC_INT,
    RC_FLOAT | RC_FRET,
    RC_FLOAT,
    RC_FLOAT,
    RC_FLOAT,
    RC_FLOAT,
    RC_FLOAT,
    RC_FLOAT,
    RC_FLOAT
};

static int wasm32_current_ret_vt = WASM32_VT_VOID;
static int wasm32_last_jump;

static const char *wasm32_dup_tok_name(int token)
{
    int base = token & ~SYM_FIELD;
    if (base >= TOK_IDENT && base < tok_ident &&
        table_ident && table_ident[base - TOK_IDENT]) {
        char *name = tcc_strdup(get_tok_str(base, NULL));
        dynarray_add(&tcc_state->wasm_names, &tcc_state->nb_wasm_names, name);
        return name;
    }
    return NULL;
}

ST_FUNC int wasm32_valtype_size(int vt)
{
    switch (vt) {
    case WASM32_VT_I64:
    case WASM32_VT_F64:
        return 8;
    case WASM32_VT_I32:
    case WASM32_VT_F32:
        return 4;
    default:
        return 0;
    }
}

ST_FUNC const char *wasm32_valtype_name(int vt)
{
    switch (vt) {
    case WASM32_VT_I32: return "i32";
    case WASM32_VT_I64: return "i64";
    case WASM32_VT_F32: return "f32";
    case WASM32_VT_F64: return "f64";
    default: return "void";
    }
}

ST_FUNC int wasm32_type_to_valtype(CType *type)
{
    int bt = type->t & VT_BTYPE;
    switch (bt) {
    case VT_VOID:
        return WASM32_VT_VOID;
    case VT_FLOAT:
        return WASM32_VT_F32;
    case VT_DOUBLE:
    case VT_LDOUBLE:
        return WASM32_VT_F64;
    case VT_LLONG:
        tcc_error("wasm32: long long lowering is not implemented");
        return WASM32_VT_I64;
    case VT_PTR:
    case VT_FUNC:
    case VT_BOOL:
    case VT_BYTE:
    case VT_SHORT:
    case VT_INT:
    case VT_ENUM:
        return WASM32_VT_I32;
    case VT_STRUCT:
        return WASM32_VT_I32; /* ABI: aggregate by pointer in first backend. */
    default:
        return WASM32_VT_I32;
    }
}

ST_FUNC void wasm32_twir_emit(uint16_t tag, uint16_t vt,
                              int a, int b, int c, int d, uint64_t imm)
{
    WasmTwirRec rec;
    int ind1;
    if (nocode_wanted)
        return;
    rec.magic = WASM32_TWIR_MAGIC;
    rec.tag = tag;
    rec.vt = vt;
    rec.a = a;
    rec.b = b;
    rec.c = c;
    rec.d = d;
    rec.imm = imm;
    ind1 = ind + sizeof rec;
    if (ind1 > cur_text_section->data_allocated)
        section_realloc(cur_text_section, ind1);
    memcpy(cur_text_section->data + ind, &rec, sizeof rec);
    ind = ind1;
}

ST_FUNC void o(unsigned int c)
{
    wasm32_twir_emit(TWIR_TRAP, WASM32_VT_VOID, c, 0, 0, 0, 0);
}

static uint64_t wasm32_const_bits(SValue *sv, int vt)
{
    if (vt == WASM32_VT_F32) {
        union { float f; uint32_t u; } u;
        u.f = sv->c.f;
        return u.u;
    }
    if (vt == WASM32_VT_F64) {
        union { double d; uint64_t u; } u;
        u.d = sv->c.d;
        return u.u;
    }
    return sv->c.i;
}

ST_FUNC void gen_fill_nops(int bytes)
{
    (void)bytes;
}

ST_FUNC void gsym_addr(int t, int a)
{
    while (t) {
        WasmTwirRec *r = (WasmTwirRec *)(cur_text_section->data + t);
        int next = r->a;
        r->b = a;
        r->a = 0;
        t = next;
    }
}

ST_FUNC int gjmp_append(int n, int t)
{
    WasmTwirRec *r;
    if (!n)
        return t;
    r = (WasmTwirRec *)(cur_text_section->data + n);
    while (r->a)
        r = (WasmTwirRec *)(cur_text_section->data + r->a);
    r->a = t;
    return n;
}

ST_FUNC int gjmp(int t)
{
    int here;
    /* In dead code no record is emitted (wasm32_twir_emit is a no-op), so we
       must not hand back the current ind as a jump handle -- it would alias
       the next live record and gsym() would corrupt it.  Mirrors riscv64. */
    if (nocode_wanted)
        return t;
    here = ind;
    wasm32_twir_emit(TWIR_JUMP, WASM32_VT_VOID, t, 0, 0, 0, 0);
    wasm32_last_jump = here;
    return here;
}

ST_FUNC void gjmp_addr(int a)
{
    wasm32_twir_emit(TWIR_JUMP, WASM32_VT_VOID, 0, a, 0, 0, 0);
}

ST_FUNC int gjmp_cond(int op, int t)
{
    /* vtop is VT_CMP: the two operand registers were stashed in cmp_r by
       gen_opi().  Record both operands + the comparison op; wasm32-emit.c
       re-materialises "a <op> b" at the branch site.  a=patch chain (t),
       b=target (patched by gsym), c=lhs reg, d=op, imm=rhs reg. */
    int a = vtop->cmp_r & 0xff;
    int b = (vtop->cmp_r >> 8) & 0xff;
    int here;
    if (nocode_wanted)
        return t;
    here = ind;
    wasm32_twir_emit(TWIR_JUMP_IF, WASM32_VT_I32, t, 0, a, op, (uint64_t)b);
    wasm32_last_jump = here;
    return here;
}

static int lvalue_slot(SValue *sv)
{
    int v = sv->r & VT_VALMASK;
    if (v == VT_LOCAL || v == VT_LLOCAL)
        return sv->c.i;
    return 0x7fffffff;
}

ST_FUNC void load(int r, SValue *sv)
{
    int fr = sv->r;
    int v = fr & VT_VALMASK;
    int vt = wasm32_type_to_valtype(&sv->type);

    if (fr & VT_LVAL) {
        int slot = lvalue_slot(sv);
        if (slot != 0x7fffffff) {
            wasm32_twir_emit(TWIR_GET_SLOT, vt, r, slot, vt, 0, 0);
            return;
        }
        if (v < VT_CONST) {
            wasm32_twir_emit(TWIR_LOAD_MEM, vt, r, v, vt, sv->c.i, 0);
            return;
        }
        if (v == VT_CONST && !(fr & VT_SYM)) {
            /* Absolute wasm linear-memory address.  This is useful in tests and
               for host-provided pointers. */
            wasm32_twir_emit(TWIR_SET_CONST, WASM32_VT_I32, r, WASM32_VT_I32, 0, 0, sv->c.i);
            wasm32_twir_emit(TWIR_LOAD_MEM, vt, r, r, vt, 0, 0);
            return;
        }
        tcc_error("wasm32: unsupported symbolic lvalue load");
    }

    if (v == VT_CONST) {
        if (fr & VT_SYM)
            tcc_error("wasm32: symbolic constants are emitted as imports/globals later");
        wasm32_twir_emit(TWIR_SET_CONST, vt, r, vt, 0, 0, wasm32_const_bits(sv, vt));
    } else if (v < VT_CONST) {
        wasm32_twir_emit(TWIR_MOV, vt, r, v, vt, 0, 0);
    } else if (v == VT_CMP) {
        /* Materialise a deferred comparison as a 0/1 value: emit "a <op> b"
           into the destination register (used when the boolean is consumed as
           a value rather than a branch condition). */
        int ca = sv->cmp_r & 0xff;
        int cb = (sv->cmp_r >> 8) & 0xff;
        wasm32_twir_emit(TWIR_BIN, WASM32_VT_I32, r, ca, cb, sv->cmp_op, 0);
    } else if (v == VT_JMP || v == VT_JMPI) {
        int done;
        wasm32_twir_emit(TWIR_SET_CONST, WASM32_VT_I32, r, WASM32_VT_I32, 0, 0, v == VT_JMPI);
        done = gjmp(0);
        gsym(sv->c.i);
        wasm32_twir_emit(TWIR_SET_CONST, WASM32_VT_I32, r, WASM32_VT_I32, 0, 0, v != VT_JMPI);
        gsym(done);
    } else {
        tcc_error("wasm32: unsupported load form %x", fr);
    }
}

ST_FUNC void store(int r, SValue *sv)
{
    int fr = sv->r;
    int v = fr & VT_VALMASK;
    int vt = wasm32_type_to_valtype(&sv->type);
    int slot;
    if (!(fr & VT_LVAL))
        tcc_error("wasm32: store target is not an lvalue");
    slot = lvalue_slot(sv);
    if (slot != 0x7fffffff) {
        wasm32_twir_emit(TWIR_SET_SLOT, vt, slot, r, vt, 0, 0);
    } else if (v < VT_CONST) {
        wasm32_twir_emit(TWIR_STORE_MEM, vt, v, r, vt, sv->c.i, 0);
    } else {
        tcc_error("wasm32: unsupported store form %x", fr);
    }
}

static int cmp_result_op(int op)
{
    return op >= TOK_ULT && op <= TOK_GT;
}

ST_FUNC void gen_opi(int op)
{
    int r, fr, vt;
    gv2(RC_INT, RC_INT);
    r = vtop[-1].r;
    fr = vtop[0].r;
    vt = wasm32_type_to_valtype(&vtop[-1].type);
    if (cmp_result_op(op)) {
        /* Defer the comparison (VT_CMP) instead of materialising a 0/1, so
           tccgen can fold it into a conditional branch via gjmp_cond().
           Operand registers are stashed in cmp_r, mirroring riscv64/arm64;
           the compare is emitted at the branch (JUMP_IF) or, when the boolean
           is used as a value, through the VT_CMP path in load(). */
        vtop--;
        vset_VT_CMP(op);
        vtop->cmp_r = (r & 0xff) | ((fr & 0xff) << 8);
        return;
    }
    wasm32_twir_emit(TWIR_BIN, vt, r, r, fr, op, 0);
    vtop--;
    vtop->r = r;
}

ST_FUNC void gen_opf(int op)
{
    int r, fr, vt, dst;
    if (op == TOK_NEG) {
        r = gv(RC_FLOAT);
        wasm32_twir_emit(TWIR_UNARY, wasm32_type_to_valtype(&vtop->type), r, r, TWIR_U_NEG,
                         wasm32_type_to_valtype(&vtop->type), 0);
        return;
    }
    gv2(RC_FLOAT, RC_FLOAT);
    r = vtop[-1].r;
    fr = vtop[0].r;
    vt = wasm32_type_to_valtype(&vtop[-1].type);
    /* Materialise float comparisons before tccgen inverts a condition.
       !(a < b) is not a >= b when either operand is NaN.  Testing the i32
       result through gen_opi preserves that distinction without FP flags. */
    dst = cmp_result_op(op) ? get_reg(RC_INT) : r;
    wasm32_twir_emit(TWIR_BIN, vt, dst, r, fr, op, 0);
    vtop--;
    vtop->r = dst;
    if (cmp_result_op(op)) {
        vtop->type.t = VT_INT;
        /* gvtst_set also calls us to test a bare float against zero and
           requires VT_CMP (or a constant), not a materialised register. */
        vpushi(0);
        gen_opi(TOK_NE);
    }
}

ST_FUNC void gen_cvt_itof(int t)
{
    int src_vt = wasm32_type_to_valtype(&vtop->type);
    int dst_vt = (t & VT_BTYPE) == VT_FLOAT ? WASM32_VT_F32 : WASM32_VT_F64;
    int r = gv(RC_INT);
    int dst = get_reg(RC_FLOAT);
    wasm32_twir_emit(TWIR_UNARY, src_vt, dst, r,
                     (vtop->type.t & VT_UNSIGNED) ? TWIR_U_CONVERT_U : TWIR_U_CONVERT_S,
                     dst_vt, 0);
    vtop->r = dst;
}

ST_FUNC void gen_cvt_ftoi(int t)
{
    int src_vt = wasm32_type_to_valtype(&vtop->type);
    int r = gv(RC_FLOAT);
    int dst = get_reg(RC_INT);
    if ((t & VT_BTYPE) == VT_LLONG)
        tcc_error("wasm32: long long lowering is not implemented");
    wasm32_twir_emit(TWIR_UNARY, src_vt, dst, r,
                     (t & VT_UNSIGNED) ? TWIR_U_TRUNC_U : TWIR_U_TRUNC_S,
                     WASM32_VT_I32, 0);
    vtop->r = dst;
}

ST_FUNC void gen_cvt_ftof(int t)
{
    int src_vt = wasm32_type_to_valtype(&vtop->type);
    int dst_vt = (t & VT_BTYPE) == VT_FLOAT ? WASM32_VT_F32 : WASM32_VT_F64;
    int r = gv(RC_FLOAT);
    if (src_vt != dst_vt)
        wasm32_twir_emit(TWIR_UNARY, src_vt, r, r,
                         dst_vt == WASM32_VT_F64 ? TWIR_U_PROMOTE : TWIR_U_DEMOTE,
                         dst_vt, 0);
    vtop->r = r;
}

ST_FUNC int gfunc_sret(CType *vt, int variadic, CType *ret, int *align, int *regsize)
{
    int a, size = type_size(vt, &a);
    (void)variadic;
    *align = a;
    *regsize = size <= 4 ? 4 : 8;
    if ((vt->t & VT_BTYPE) == VT_STRUCT || size > 8)
        return 0;
    ret->t = vt->t & VT_BTYPE;
    ret->ref = NULL;
    return size > 0;
}

ST_FUNC void gfunc_prolog(Sym *func_sym)
{
    CType *func_type = &func_sym->type;
    Sym *sym = func_type->ref;
    int i = 0;
    func_vc = 0;
    loc = 0;
    wasm32_current_ret_vt = wasm32_type_to_valtype(&func_vt);
    wasm32_twir_emit(TWIR_FUNC_BEGIN, wasm32_current_ret_vt,
                     func_sym->v & ~SYM_FIELD, wasm32_current_ret_vt, func_var, 0,
                     (uint64_t)(uintptr_t)wasm32_dup_tok_name(func_sym->v));
    while ((sym = sym->next) != NULL) {
        int vt = wasm32_type_to_valtype(&sym->type);
        int slot = i;
        if (i == WASM32_MAX_ARGS)
            tcc_error("wasm32: at most %d parameters are supported", WASM32_MAX_ARGS);
        wasm32_twir_emit(TWIR_PARAM, vt, slot, vt, i, 0, 0);
        gfunc_set_param(sym, slot, 0);
        ++i;
    }
}

ST_FUNC void gfunc_epilog(void)
{
    if (wasm32_current_ret_vt == WASM32_VT_VOID)
        wasm32_twir_emit(TWIR_RETURN, WASM32_VT_VOID, 0, WASM32_VT_VOID, 0, 0, 0);
    else
        wasm32_twir_emit(TWIR_RETURN, wasm32_current_ret_vt,
                         (wasm32_current_ret_vt == WASM32_VT_F32 || wasm32_current_ret_vt == WASM32_VT_F64) ? REG_FRET : REG_IRET,
                         wasm32_current_ret_vt, 0, 0, 0);
    wasm32_twir_emit(TWIR_FUNC_END, WASM32_VT_VOID, 0, 0, 0, 0, 0);
}

ST_FUNC void gfunc_call(int nb_args)
{
    SValue *func = vtop - nb_args;
    int i, token = 0, ret_vt = WASM32_VT_I32;
    CType *ft;
    /* Preserve values such as the first result in g(a)+g(b) across the call.
       Scalar spills use wasm locals, not an addressable shadow stack. */
    if (nb_args > WASM32_MAX_ARGS)
        tcc_error("wasm32: at most %d arguments are supported", WASM32_MAX_ARGS);
    save_regs(nb_args + 1);
    if (!((func->r & VT_SYM) && ((func->r & (VT_VALMASK | VT_LVAL)) == VT_CONST)))
        tcc_error("wasm32: only direct symbolic calls are implemented in first call ABI");
    token = func->sym->v & ~SYM_FIELD;
    ft = &func->type;
    if (func->sym && !func->sym->c)
        put_extern_sym(func->sym, NULL, 0, 0);
    if ((ft->t & VT_BTYPE) == VT_FUNC && ft->ref)
        ret_vt = wasm32_type_to_valtype(&ft->ref->type);
    for (i = 0; i < nb_args; ++i) {
        SValue *a = func + 1 + i;
        int vt = wasm32_type_to_valtype(&a->type);
        /* Earlier CALL_ARG values already live on the wasm operand stack.
           Use a free scratch register, not argument-position registers that
           can overwrite a later argument's still-live source. */
        int ar = get_reg(vt == WASM32_VT_F32 || vt == WASM32_VT_F64 ? RC_FLOAT : RC_INT);
        load(ar, a);
        wasm32_twir_emit(TWIR_CALL_ARG, vt, ar, vt, i, 0, 0);
    }
    wasm32_twir_emit(TWIR_CALL, ret_vt, token, nb_args, ret_vt,
                     (ret_vt == WASM32_VT_F32 || ret_vt == WASM32_VT_F64) ? REG_FRET : REG_IRET,
                     (uint64_t)(uintptr_t)wasm32_dup_tok_name(token));
    /* Pop arguments and callee.  tccgen.c pushes the architectural return
       register value immediately after gfunc_call() returns. */
    vtop = func - 1;
}

ST_FUNC void ggoto(void)
{
    tcc_error("wasm32: computed goto is not implemented");
}

ST_FUNC void gen_vla_sp_save(int addr)
{
    (void)addr;
}

ST_FUNC void gen_vla_sp_restore(int addr)
{
    (void)addr;
}

ST_FUNC void gen_vla_alloc(CType *type, int align)
{
    (void)type;
    (void)align;
    tcc_error("wasm32: VLA allocation requires stack-pointer import pass");
}

#endif
