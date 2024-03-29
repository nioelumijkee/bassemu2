/* (c) 2006 Ch. Klippel
 * this software is gpl'ed software, read the file "LICENSE.txt" for details
 */

#include "m_pd.h"
#include <math.h>
#include "include/saw441.h"
#include "include/pls441.h"

// -------------------------------------------------------------------------- //
static t_class *be2_class;

#define PI   	   3.141592653589793
#define PI_2   	   6.283185307179586
enum {
  VCO_SAW=0,
  VCO_RECT,
  VCO_SSAW,
  VCO_SRECT,
  VCO_EXT,
};
#define VCA_ATT    0
#define VCA_DEC    1
#define VCA_OFF    2
#define HPFREQ     50.0
#define FEGLPFREQ  300.0
#define SMFREQ     22.0
#define ACCAEGLPFREQ  50.0


typedef struct _be2
{
  t_object x_obj;
  t_float sr; /* sample rate */

  // reset vco, vcf
  int rst;
  
  // smooth
  t_float sm_f; 

  // in
  t_float in_level;

  // hp filter
  t_float hp_f;
  t_float hp_z;
  
  // vco
  t_float vco_inc;
  t_float vco_sig;
  t_float vco_count;
  t_float vco_pw;
  int     vco_type;
  t_float vco_pitch;
  t_float vco_tune;

  // vcf
  t_float vcf_cut;
  t_float vcf_env;
  t_float vcf_envdecay;
  t_float vcf_decay;
  t_float vcf_res;
  t_float vcf_rcf;
  t_float vcf_a; /* coef */
  t_float vcf_b;
  t_float vcf_c;
  t_float vcf_eg; /* filter envelope */
  t_float vcf_d1; /* z */
  t_float vcf_d2;
  t_float vcf_e0; /* coef */
  t_float vcf_e1;
  t_float vcf_acor; /* res -> a cor */
  t_float vcf_eg_lp_z; /* feg lp */
  t_float vcf_eg_lp_f;
  t_float vcf_cut_z;/* smooth filters */
  t_float vcf_res_z;
  t_float vcf_rcf_z;
  t_float vcf_env_z;

  // vca
  t_float vca_attack; /* vca */
  t_float vca_decay;
  t_float vca_a;
  t_float vca_a0;
  int     vca_mode;
  t_float vca_dec;

  // accent
  t_float decay;
  t_float accent;
  t_float accent_to_amp;
  int     acc;
  t_float acc_eg_lp_z;
  t_float acc_eg_lp_f;
} t_be2;


// -------------------------------------------------------------------------- //
// calc ////////////////////////////////////////////////////////////////////////
// -------------------------------------------------------------------------- //
static void be2_calc_vco_inc(t_be2 *x)
{
  t_float p = x->vco_pitch - 60.;
  p = p * 0.0833334;
  p = exp2(p);
  p = p * x->vco_tune;
  p = (1. / x->sr) * p;
  if (p > 0.5) p = 0.5;
  x->vco_inc = p;
}

static void be2_calc_decay(t_be2 *x)
{
  t_float f = x->vcf_decay * x->sr;
  x->vcf_envdecay = pow(0.1, (1.0 / f));
}

static void be2_calc_rel(t_be2 *x)
{
  t_float f = (x->vca_dec * 0.15) + 0.001;
  x->vca_decay  = ((1.0 / x->sr) / f); // sec
}

static void be2_calc_acc(t_be2 *x)
{
  t_float f;
  if (x->acc)
    {
      x->vcf_decay = 0.06;
      x->accent_to_amp = x->accent * 5;
    }
  else
    {
      f = x->decay;
      f = f*f*f*f;
      f = 0.06 + (1.5 * f);
      x->vcf_decay = f;
      x->accent_to_amp = 0;
    }
}

static void be2_reset(t_be2 *x)
{
  x->vca_mode = VCA_OFF;
  x->vca_a = 0.0;
  x->vca_a0 = 0.5;
  x->vca_attack = 1.0 - ((1.0 / x->sr) / 0.0008); // sec
  x->hp_f = (PI_2 / x->sr) * HPFREQ;
  x->hp_z = 0.0;
  x->vcf_eg_lp_f = (PI_2 / x->sr) * FEGLPFREQ;
  x->vcf_eg_lp_z = 0.0;
  x->sm_f = (PI_2 / x->sr) * SMFREQ;
  x->vcf_cut_z = 0.0;
  x->vcf_res_z = 0.0;
  x->vcf_rcf_z = 0.0;
  x->vcf_env_z = 0.0;
  x->acc_eg_lp_f = (PI_2 / x->sr) * ACCAEGLPFREQ;
  x->acc_eg_lp_z = 0.0;
}

// -------------------------------------------------------------------------- //
// input methods ///////////////////////////////////////////////////////////////
// -------------------------------------------------------------------------- //
static void be2_gate(t_be2 *x, t_floatarg f)
{
  if(f > 0)
    {
      x->vca_mode = VCA_ATT;
      x->vcf_eg = x->vcf_e1;
      if (x->rst)
        {
          x->vcf_d1 = 0.0;
          x->vcf_d2 = 0.0;
          x->vco_count = 0.0;
        }
    }
  else
    {
      x->vca_mode = VCA_DEC;
    }
}

static void be2_pitch(t_be2 *x, t_floatarg f)
{
  x->vco_pitch = f;
  be2_calc_vco_inc(x);
}

static void be2_vco(t_be2 *x, t_floatarg f)
{
  x->vco_type = f;
}

static void be2_tune(t_be2 *x, t_floatarg f)
{
  x->vco_tune = f;
  be2_calc_vco_inc(x);
}

static void be2_cutoff(t_be2 *x, t_floatarg f)
{
  if      (f > 1.0) f = 1.0;
  else if (f < 0.0) f = 0.0;
  x->vcf_cut = f;
}

static void be2_reso(t_be2 *x, t_floatarg f)
{
  if      (f > 1.0) f = 1.0;
  else if (f < 0.0) f = 0.0;
  f = f*1.55;
  x->vcf_res = f;
  x->vcf_rcf = exp(-1.20 + 3.455*(x->vcf_res));
}

static void be2_envmod(t_be2 *x, t_floatarg f)
{
  if      (f > 1.0) f = 1.0;
  else if (f < 0.0) f = 0.0;
  x->vcf_env = f;
}

static void be2_decay(t_be2 *x, t_floatarg f)
{
  if      (f > 1.0) f = 1.0;
  else if (f < 0.0) f = 0.0;
  x->decay = f;
  be2_calc_acc(x);
  be2_calc_decay(x);
}

static void be2_pw(t_be2 *x, t_floatarg f)
{
  if      (f > 1.0) f = 1.0;
  else if (f < 0.0) f = 0.0;
  x->vco_pw = f;
}

static void be2_rst(t_be2 *x, t_floatarg f)
{
  x->rst = f;
}

static void be2_rel(t_be2 *x, t_floatarg f)
{
  if      (f > 1.0) f = 1.0;
  else if (f < 0.0) f = 0.0;
  x->vca_dec = f*f*f;
  be2_calc_rel(x);
}

static void be2_in(t_be2 *x, t_floatarg f)
{
  x->in_level = f;
}

static void be2_accent(t_be2 *x, t_floatarg f)
{
  x->accent = f;
}

static void be2_acc(t_be2 *x, t_floatarg f)
{
  x->acc = (f>0);
  be2_calc_acc(x);
  be2_calc_decay(x);
}

// -------------------------------------------------------------------------- //
// dsp /////////////////////////////////////////////////////////////////////////
// -------------------------------------------------------------------------- //
static t_int *be2_perform(t_int *ww)
{
  t_be2 *x = (t_be2 *)(ww[1]);
  t_float *inbuf = (t_float *)(ww[2]);
  t_float *outbuf = (t_float *)(ww[3]);
  int n = (int)(ww[4]);
  
  int i0, i1;
  t_float a;
  t_float w = 0;
  t_float k = 0;
  t_float f;
  t_float sig;
  
  // only compute if needed .......
  if (x->vca_mode != VCA_OFF)
    {
      // begin be2 dsp engine
      while(n--)
        {
          // vco or ext in
          switch(x->vco_type)
            {
            case VCO_SAW :
              x->vco_count += x->vco_inc;
              if (x->vco_count > 1.0)
                x->vco_count = 0.0;
              sig = x->vco_count * 4.0;
              if (sig > 1.0) sig = 1.0;
              sig -= 0.5;
              sig *= 3.0;
              break;
              
            case VCO_RECT :
              x->vco_count += x->vco_inc;
              if (x->vco_count > 1.0)
                x->vco_count = 0.0;
              if (x->vco_count < x->vco_pw)
                sig = 1.0;
              else
                sig = -1.0;
              break;
              
            case VCO_SSAW :
              x->vco_count += x->vco_inc;
              if (x->vco_count > 1.0)
                x->vco_count = 0.0;
	      a = x->vco_count*440; // 0 ... 440
	      i0 = a;
	      i1 = i0 + 1;
	      a = a - i0;
	      sig = (saw441[i1] - saw441[i0]) * a + saw441[i0];
	      sig *= 4.0;
            break;

            case VCO_SRECT :
              x->vco_count += x->vco_inc;
              if (x->vco_count > 1.0)
                x->vco_count = 0.0;
	      a = x->vco_count*440; // 0 ... 440
	      i0 = a;
	      i1 = i0 + 1;
	      a = a - i0;
	      sig = (pls441[i1] - pls441[i0]) * a + pls441[i0];
	      sig *= 4.0;
            break;
              
            default : // VCO_EXT
              sig = *inbuf++;
              break;
            }

	  // pre
	  sig *= x->in_level;
	  
          // hpf
          x->hp_z = (sig - x->hp_z) * x->hp_f + x->hp_z;
          sig = sig - x->hp_z;
          
          // update vca
          switch(x->vca_mode)
            {
            case VCA_ATT :
              x->vca_a += (x->vca_a0 - x->vca_a) * x->vca_attack;
              break;
              
            case VCA_DEC :
              x->vca_a -= x->vca_a * x->vca_decay;
              if(x->vca_a < (0.00001))
                {
                  x->vca_a = 0;
                  x->vca_mode = VCA_OFF;
                }
              break;
              
            default : 
              break;
            }
         
          // feg and feg lp
          x->vcf_eg *= x->vcf_envdecay;
	  x->vcf_eg_lp_z = (x->vcf_eg - x->vcf_eg_lp_z)
            * x->vcf_eg_lp_f + x->vcf_eg_lp_z;

	  // acc aeg lp
	  if (x->vcf_e1 > 0)
	    {
	      f = x->vcf_eg / x->vcf_e1;
	    }
	  else
	    {
	      f = 0;
	    }
          x->acc_eg_lp_z = (f - x->acc_eg_lp_z)
	    * x->acc_eg_lp_f + x->acc_eg_lp_z;

          // smooth
          x->vcf_cut_z = (x->vcf_cut - x->vcf_cut_z) * x->sm_f + x->vcf_cut_z;
          x->vcf_res_z = (x->vcf_res - x->vcf_res_z) * x->sm_f + x->vcf_res_z;
          x->vcf_rcf_z = (x->vcf_rcf - x->vcf_rcf_z) * x->sm_f + x->vcf_rcf_z;
          x->vcf_env_z = (x->vcf_env - x->vcf_env_z) * x->sm_f + x->vcf_env_z;

	  // calc coef
	  /* x->vcf_e0 = exp(5.613 - 0.8000*(x->vcf_env_z)  */
	  x->vcf_e0 = exp(5.613 - 1.0000*(x->vcf_env_z) 
			  + 2.1553*(x->vcf_cut_z) 
			  - 0.7696*(1.0 - x->vcf_res_z));
	  x->vcf_e0 *=PI/x->sr;

	  x->vcf_e1 = exp(6.109 + 1.5876*(x->vcf_env_z) 
			  + 2.1553*(x->vcf_cut_z) 
			  - 1.2000*(1.0 - x->vcf_res_z));
	  x->vcf_e1 *=PI/x->sr;
	  x->vcf_e1 = x->vcf_e1 - x->vcf_e0;
	  if (x->vcf_e1 > 1.0) x->vcf_e1 = 1.0; // clip

	  x->vcf_acor = 1.0 - (x->vcf_res_z * 0.45); // comp res -> lvl
	  if (x->vcf_acor < 0.0) x->vcf_acor = 0.0; // clip

          // filter coef
          w = x->vcf_e0 + x->vcf_eg_lp_z;
          k = exp(-w/x->vcf_rcf_z);
          x->vcf_a = 2.0*cos(2.0*w) * k;
          x->vcf_b = -k*k;
          x->vcf_c = 1.0 - x->vcf_a - x->vcf_b;

	  // correction ampitude
          sig = (sig * x->vcf_acor);

          // filter
          f = x->vcf_a * x->vcf_d1
            +  x->vcf_b * x->vcf_d2
            +  x->vcf_c * sig;
          x->vcf_d2 = x->vcf_d1;
          x->vcf_d1 = f;

	  // vca
          f = f * x->vca_a;

	  // acc to amp
	  f += x->acc_eg_lp_z * x->accent_to_amp * f;
          
          // limit (soft-never-clip)
          /* f *= 0.5; */
          a = f;
          if (a < 0.) a = 0.-a; // abs
          a = a * 0.5;
          *(outbuf++) = f / (a + 1.);
          /* *(outbuf++) = f; */
        }
    }
  else
    while(n--)
      {
        *outbuf++ = 0.0;
      }
  
  return (ww+5);
}

static void be2_dsp(t_be2 *x, t_signal **sp)
{
  dsp_add(be2_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
  if (x->sr != sp[0]->s_sr)
    {
      x->sr = sp[0]->s_sr;
      be2_calc_vco_inc(x);
      be2_calc_decay(x);
      be2_calc_rel(x);
      be2_reset(x);
    }
}

// -------------------------------------------------------------------------- //
// setup ///////////////////////////////////////////////////////////////////////
// -------------------------------------------------------------------------- //
static void *be2_new(void)
{
  t_be2 *x = (t_be2 *)pd_new(be2_class);
  outlet_new(&x->x_obj, gensym("signal"));
  x->sr = 44100.;
  be2_calc_vco_inc(x);
  be2_calc_decay(x);
  be2_calc_rel(x);
  be2_reset(x);
  return (x);
}

void bassemu2_tilde_setup(void)
{
  be2_class=class_new(gensym("bassemu2~"),
                      (t_newmethod)be2_new,0,sizeof(t_be2),0,A_GIMME,0);
  class_addmethod(be2_class, nullfn, gensym("signal"), 0);
  class_addmethod(be2_class,(t_method)be2_dsp,gensym("dsp"), 0);
  class_addmethod(be2_class,(t_method)be2_gate,gensym("gate"),A_DEFFLOAT, 0);
  class_addmethod(be2_class,(t_method)be2_pitch,gensym("pitch"),A_DEFFLOAT, 0);
  class_addmethod(be2_class,(t_method)be2_tune,gensym("tune"),A_DEFFLOAT, 0);
  class_addmethod(be2_class,(t_method)be2_vco,gensym("vco"),A_DEFFLOAT, 0);
  class_addmethod(be2_class,(t_method)be2_cutoff,gensym("cutoff"),A_DEFFLOAT, 0);
  class_addmethod(be2_class,(t_method)be2_reso,gensym("reso"),A_DEFFLOAT, 0);
  class_addmethod(be2_class,(t_method)be2_envmod,gensym("envmod"),A_DEFFLOAT, 0);
  class_addmethod(be2_class,(t_method)be2_decay,gensym("decay"),A_DEFFLOAT, 0);
  class_addmethod(be2_class,(t_method)be2_pw,gensym("pw"),A_DEFFLOAT, 0);
  class_addmethod(be2_class,(t_method)be2_rst,gensym("rst"),A_DEFFLOAT,0);
  class_addmethod(be2_class,(t_method)be2_rel,gensym("rel"),A_DEFFLOAT,0);
  class_addmethod(be2_class,(t_method)be2_in,gensym("in"),A_DEFFLOAT,0);
  class_addmethod(be2_class,(t_method)be2_accent,gensym("accent"),A_DEFFLOAT,0);
  class_addmethod(be2_class,(t_method)be2_acc,gensym("acc"),A_DEFFLOAT,0);
  class_addmethod(be2_class,(t_method)be2_reset,gensym("reset"),0);
}

