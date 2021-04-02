 /* (c) 2006 Ch. Klippel
 * this software is gpl'ed software, read the file "LICENSE.txt" for details
 */

#include "m_pd.h"
#include "math.h"

// --------------------------------------------------------------------------- #
static t_class *bassemu2_class;

#define PI_2   	 6.282185
#define SINFACT  12.56437
#define VCO_SAW  0
#define VCO_RECT 1
#define VCO_EXT  2
#define ENV_INC  8
#define VCA_ATT  0
#define VCA_DEC  1
#define VCA_SIL  2
#define HPFREQ   100.0


typedef struct _bassemu2
{
  t_object x_obj;
  float vco_inc;
  float cur_wave;
  float ideal_wave;
  float hp_f;
  float hp_z;
  float vco_count;
  float pw;
  int vco_type;
  float pitch;
  float tune;
  float vcf_cutoff;
  float vcf_envmod;
  float vcf_envdecay;
  float vcf_reso;
  float vcf_rescoeff;
  float vcf_a;
  float vcf_b;
  float vcf_c;
  float vcf_c0;
  float vcf_d1;
  float vcf_d2;
  float vcf_e0;
  float vcf_e1;
  float vcf_acor;
  int vcf_envpos;
  float vca_attack;
  float vca_decay;
  float vca_a;
  float vca_a0;
  int vca_mode;
  float sr;
} t_bassemu2;


// --------------------------------------------------------------------------- #
static void bassemu2_recalc(t_bassemu2 *x)
{
  x->vcf_e1 = exp(6.109 + 1.5876*(x->vcf_envmod) + 2.1553*(x->vcf_cutoff) - 1.2*(1.0-x->vcf_reso));
  x->vcf_e0 = exp(5.613 - 0.8*(x->vcf_envmod) + 2.1553*(x->vcf_cutoff) - 0.7696*(1.0-x->vcf_reso));
  x->vcf_e0 *=M_PI/x->sr;
  x->vcf_e1 *=M_PI/x->sr;
  x->vcf_e1 -= x->vcf_e0;
  if (x->vcf_e1 > 1.0) x->vcf_e1 = 1.0; // clip
  x->vcf_envpos = ENV_INC;
  x->vcf_acor = 1.0 - (x->vcf_reso * 0.45); // comp res -> lvl
}

// --------------------------------------------------------------------------- #
static void bassemu2_calc_vco_inc(t_bassemu2 *x)
{
  float p = x->pitch - 60.;
  p = p * 0.0833334;
  p = exp2(p);
  p = p * x->tune;
  p = (1. / x->sr) * p;
  if (p > 0.5) p = 0.5;
  x->vco_inc = p;
}


// --------------------------------------------------------------------------- #
static void bassemu2_gate(t_bassemu2 *x, t_floatarg f)
{
  if(f > 0)
    {
      x->vca_mode = VCA_ATT;
      x->vcf_c0 = x->vcf_e1;
      x->vcf_envpos = ENV_INC;
    }
  else
    {
      x->vca_mode = VCA_DEC;
    }
}

// --------------------------------------------------------------------------- #
static void bassemu2_pitch(t_bassemu2 *x, t_floatarg f)
{
  x->pitch = f;
  bassemu2_calc_vco_inc(x);
}

// --------------------------------------------------------------------------- #
static void bassemu2_vco(t_bassemu2 *x, t_floatarg f)
{
  x->vco_type = f;
}

// --------------------------------------------------------------------------- #
static void bassemu2_tune(t_bassemu2 *x, t_floatarg f)
{
  x->tune = f;
  bassemu2_calc_vco_inc(x);
}

// --------------------------------------------------------------------------- #
static void bassemu2_cutoff(t_bassemu2 *x, t_floatarg f)
{
  if      (f > 1.0) f = 1.0;
  else if (f < 0.0) f = 0.0;
  x->vcf_cutoff = f;
  bassemu2_recalc(x);
}

// --------------------------------------------------------------------------- #
static void bassemu2_reso(t_bassemu2 *x, t_floatarg f)
{
  if      (f > 1.0) f = 1.0;
  else if (f < 0.0) f = 0.0;
  f = f*1.42;
  x->vcf_reso = f;
  x->vcf_rescoeff = exp(-1.20 + 3.455*(x->vcf_reso));
  bassemu2_recalc(x);
}

// --------------------------------------------------------------------------- #
static void bassemu2_envmod(t_bassemu2 *x, t_floatarg f)
{
  if      (f > 1.0) f = 1.0;
  else if (f < 0.0) f = 0.0;
  x->vcf_envmod = f;
  bassemu2_recalc(x);
}

// --------------------------------------------------------------------------- #
static void bassemu2_decay(t_bassemu2 *x, t_floatarg f)
{
  if      (f > 1.0) f = 1.0;
  else if (f < 0.0) f = 0.0;
  f = f*f*f;
  f = 0.044 + (2.2 * f);
  f = f * x->sr;
  x->vcf_envdecay = pow(0.1, ((1.0 / f) * ENV_INC));
}

// --------------------------------------------------------------------------- #
static void bassemu2_pw(t_bassemu2 *x, t_floatarg f)
{
  if      (f > 1.0) f = 1.0;
  else if (f < 0.0) f = 0.0;
  x->pw = f;
}

// --------------------------------------------------------------------------- #
static void bassemu2_reset(t_bassemu2 *x)
{
  x->vcf_a = 0.0;
  x->vcf_b = 0.0;
  x->vcf_c = 0.0;
  x->vcf_envpos = 0;
  x->vca_mode = VCA_SIL;
  x->vca_a = 0.0;
  x->vca_a0 = 0.5;
  x->vca_attack = 1.0 - 0.94406088;
  x->vca_decay  = 0.96797516; // <- decay
  x->hp_f = (PI_2 / x->sr) * HPFREQ;
  x->hp_z = 0.0;
}

// --------------------------------------------------------------------------- #
static t_int *bassemu2_perform(t_int *ww)
{
  t_bassemu2 *x = (t_bassemu2 *)(ww[1]);
  t_float *inbuf = (t_float *)(ww[2]);
  t_float *outbuf = (t_float *)(ww[3]);
  int n = (int)(ww[4]);
  
  float a;
  float w = 0;
  float k = 0;
  float ts;
  
  // only compute if needed .......
  if (x->vca_mode != VCA_SIL)
    {
      // begin bassemu2 dsp engine
      while(n--)
	{

	  // update vcf
	  if(x->vcf_envpos >= ENV_INC)
	    {
	      w = x->vcf_e0 + x->vcf_c0;
	      k = exp(-w/x->vcf_rescoeff);
	      x->vcf_c0 *= x->vcf_envdecay;
	      x->vcf_a = 2.0*cos(2.0*w) * k;
	      x->vcf_b = -k*k;
	      x->vcf_c = 1.0 - x->vcf_a - x->vcf_b;
	      x->vcf_envpos = 0;
	    }
	  
	  // vco or ext in
	  switch(x->vco_type)
	    {
	    case VCO_SAW :
	      x->ideal_wave = sin(x->vco_count);
	      x->vco_count += x->vco_inc;
	      if( x->vco_count <= 0.0 )
		x->cur_wave = (x->cur_wave + ((x->ideal_wave - x->cur_wave) * 0.95));
	      else
		x->cur_wave = (x->cur_wave + ((x->ideal_wave - x->cur_wave) * 0.9));
	      if (x->vco_count > 0.5)
		x->vco_count = -0.5;
	      break;
	      
	    case VCO_RECT :
	      if ((x->vco_count+0.5) <= x->pw)
		x->ideal_wave = -0.5;
	      else
		x->ideal_wave = 0.5;
	      x->vco_count += x->vco_inc;
	      if( x->vco_count <= 0.0 )
		x->cur_wave = (x->cur_wave + ((x->ideal_wave - x->cur_wave) * 0.95));
	      else
		x->cur_wave = (x->cur_wave + ((x->ideal_wave - x->cur_wave) * 0.9));
	      if (x->vco_count > 0.5)
		x->vco_count = -0.5;
	      x->cur_wave *= 0.8;
	      break;
	      
	    case VCO_EXT :
	      x->cur_wave = (*inbuf++ * 0.48);
	      // clip
	      if (x->cur_wave < -0.48) x->cur_wave = -0.48;
	      if (x->cur_wave > 0.48)  x->cur_wave = 0.48;
	      break;

	    default : 
	      break;
	    }
	  
	  
	  // hpf
	  x->hp_z = (x->cur_wave - x->hp_z) * x->hp_f + x->hp_z;
	  ts = x->cur_wave - x->hp_z;

	  
	  // update vca
	  switch(x->vca_mode)
	    {
	    case VCA_ATT :
	      x->vca_a += (x->vca_a0 - x->vca_a) * x->vca_attack;
	      break;

	    case VCA_DEC :
	      x->vca_a *= x->vca_decay;
	      if(x->vca_a < (1/65536.0))
		{
		  x->vca_a = 0;
		  x->vca_mode = VCA_SIL;
		}
	      break;

	    default : 
	      break;
	    }

	  // compute sample
	  ts = ts * x->vca_a;
	  ts = ts * x->vcf_acor;
	  ts = x->vcf_a * x->vcf_d1 + x->vcf_b * x->vcf_d2 + x->vcf_c * ts;
	  x->vcf_d2 = x->vcf_d1;
	  x->vcf_envpos++;
	  x->vcf_d1 = ts;

	  // limit (soft-never-clip)
	  ts *= 1.25;
	  a = ts;
	  if (a < 0.) a = 0.-a; // abs
	  a = a * 0.8;
	  *(outbuf++) = ts / (a + 1.);
	  
	}
    } //end vcamode != 2
  else
    while(n--)
      {
	*outbuf++ = 0.0;
      }
  
  return (ww+5);
}

// --------------------------------------------------------------------------- #
static void bassemu2_dsp(t_bassemu2 *x, t_signal **sp)
{
  dsp_add(bassemu2_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
  x->sr = sp[0]->s_sr;
  bassemu2_calc_vco_inc(x);
  bassemu2_recalc(x);
}

// --------------------------------------------------------------------------- #
static void *bassemu2_new(void)
{
  t_bassemu2 *x = (t_bassemu2 *)pd_new(bassemu2_class);
  outlet_new(&x->x_obj, gensym("signal"));
  x->sr = 44100.;
  bassemu2_reset(x);
  return (x);
}

// --------------------------------------------------------------------------- #
void bassemu2_tilde_setup(void)
{
  bassemu2_class=class_new(gensym("bassemu2~"),(t_newmethod)bassemu2_new,0,sizeof(t_bassemu2),0,A_GIMME,0);
  class_addmethod(bassemu2_class, nullfn, gensym("signal"), 0);
  class_addmethod(bassemu2_class,(t_method)bassemu2_dsp,gensym("dsp"), 0);
  class_addmethod(bassemu2_class,(t_method)bassemu2_gate,gensym("gate"),A_DEFFLOAT, 0);
  class_addmethod(bassemu2_class,(t_method)bassemu2_pitch,gensym("pitch"),A_DEFFLOAT, 0);
  class_addmethod(bassemu2_class,(t_method)bassemu2_tune,gensym("tune"),A_DEFFLOAT, 0);
  class_addmethod(bassemu2_class,(t_method)bassemu2_vco,gensym("vco"),A_DEFFLOAT, 0);
  class_addmethod(bassemu2_class,(t_method)bassemu2_cutoff,gensym("cutoff"),A_DEFFLOAT, 0);
  class_addmethod(bassemu2_class,(t_method)bassemu2_reso,gensym("reso"),A_DEFFLOAT, 0);
  class_addmethod(bassemu2_class,(t_method)bassemu2_envmod,gensym("envmod"),A_DEFFLOAT, 0);
  class_addmethod(bassemu2_class,(t_method)bassemu2_decay,gensym("decay"),A_DEFFLOAT, 0);
  class_addmethod(bassemu2_class,(t_method)bassemu2_pw,gensym("pw"),A_DEFFLOAT, 0);
  class_addmethod(bassemu2_class,(t_method)bassemu2_reset,gensym("reset"),0);
}

