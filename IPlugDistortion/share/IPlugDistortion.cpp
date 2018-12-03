/*

 IPlug distortion example
 (c) Theo Niessink 2011
 <http://www.taletn.com/>

 This software is provided 'as-is', without any express or implied
 warranty. In no event will the authors be held liable for any damages
 arising from the use of this software.

 Permission is granted to anyone to use this software for any purpose,
 including commercial applications, and to alter it and redistribute it
 freely, subject to the following restrictions:

 1. The origin of this software must not be misrepresented; you must not
 claim that you wrote the original software. If you use this software in a
 product, an acknowledgment in the product documentation would be
 appreciated but is not required.
 2. Altered source versions must be plainly marked as such, and must not be
 misrepresented as being the original software.
 3. This notice may not be removed or altered from any source distribution.


 Simple IPlug audio effect that shows how to implement oversampling to reduce
 aliasing.

 */
#include "IPlugDistortion.h"
#include "IPlug_include_in_plug_src.h"
#include "IAutoGUI.h"

#include <math.h>
#include "../../WDL/denormal.h"
enum ELayout
{
    kW = GUI_WIDTH,  // width of plugin window
    kH = GUI_HEIGHT,	// height of plugin window

    kISwitchControl_2_N = 2,  // # of sub-bitmaps.
    kISwitchControl_2_X = 64,  // position of left side of control
    kISwitchControl_2_Y = 72,  // position of top of control

    kISwitchControl_3_N = 3,
    kISwitchControl_3_X = 216,
    kISwitchControl_3_Y = 72,

    kIInvisibleSwitchControl_X = 216,
    kIInvisibleSwitchControl_Y = 192,
    kIISC_W = 48,   // width of control
    kIISC_H = 48,	// height of control

    kIISC_I_X = 75, // position of InvisibleSwitch indicator graphic
    kIISC_I_Y = 203,

    kIRadioButtonsControl_N = 2,
    kIRBC_W = 24,  // width of bitmap
    kIRBC_H = 24,  // height of one of the bitmap images
    kIRadioButtonsControl_H_X = 23,
    kIRadioButtonsControl_H_Y = 320,
    kIRBC_HN = 4,  // number of horizontal buttons
    kIRadioButtonsControl_V_X = 228,
    kIRadioButtonsControl_V_Y = 285,
    kIRBC_VN = 3,  // number of vertical buttons

    kIContactControl_N = 2,
    kIContactControl_X = 136,
    kIContactControl_Y = 424,

    kIFaderControl_Horiz_L = 200,  // fader length
    kIFaderControl_Horiz_X = 0,
    kIFaderControl_Horiz_Y = 544,

    kIFaderControl_Vert_L = 100,
    kIFaderControl_Vert_X = 216,
    kIFaderControl_Vert_Y = 512,

    kIKLC_W = 48, //knob area horizontal size
    kIKLC_H = 48, //knob area vertical size
    kIKnobLineControl_def_X = 392,
    kIKnobLineControl_def_Y = 72,

    kIKnobLineControl_lo_gear_X = 576,
    kIKnobLineControl_lo_gear_Y = 72,

    kIKnobRotaterControl_def_X = 384,
    kIKnobRotaterControl_def_Y = 184,

    kIKnobRotaterControl_restrict_X = 568,
    kIKnobRotaterControl_restrict_Y = 184,

    kIKnobMultiControl_N = 14,
    kIKnobMultiControl_def_X = 376,
    kIKnobMultiControl_def_Y = 296,

    kIKnobMultiControl_Horiz_X = 560,
    kIKnobMultiControl_Horiz_Y = 296,

    //IKnobRotatingMaskControl
    kIKRMC_X = 575,
    kIKRMC_Y = 436,

    //IBitmapOverlayControl
    kIBOC_X = 8,	//top left position of bitmap overlay
    kIBOC_Y = 400,
    kIBOC_T_X = 576, //top left position of target area
    kIBOC_T_Y = 552,
    kIBOC_T_W = 48, //width of target area (square in this example so no height required)

                    //ITextControl
                    kITC_X = 784,
                    kITC_Y = 87,
                    kITC_W = 100,
                    kITC_H = 20,

                    //ICaptionControl
                    kICC_X = 784,
                    kICC_Y = 202,
                    kICC_W = 100,
                    kICC_H = 20,

                    //IURLControl
                    kIUC_X = 824,
                    kIUC_Y = 312,
                    kIUC_W = 48

};

inline double fast_tanh(double x)
{
  x = exp(x + x);
  return (x - 1) / (x + 1);
}


IPlugDistortion::IPlugDistortion(IPlugInstanceInfo instanceInfo):
  IPLUG_CTOR(kNumParams, 0, instanceInfo),
  mOversampling(8), mDC(0.2)
{
  TRACE;
  mAntiAlias.Calc(0.5 / (double)mOversampling);
  mUpsample.Reset();
  mDownsample.Reset();

  mDistortedDC = fast_tanh(mDC);

  GetParam(kDrive)->InitDouble("Drive", 0.5, 0., 1., 0.001);
  
  IGraphics* pGraphics = MakeGraphics(this, GUI_WIDTH, GUI_HEIGHT);
  //IText textProps(12, &COLOR_BLACK, "Verdana", IText::kStyleNormal, IText::kAlignNear, 0, IText::kQualityNonAntiAliased);
  //GenerateKnobGUI(pGraphics, this, &textProps, &COLOR_WHITE, &COLOR_BLACK, 60, 70);
  //pGraphics->AttachPanelBackground(&COLOR_GREEN);
  pGraphics->AttachBackground(BG_ID, BG_FN);
  IBitmap bitmap = pGraphics->LoadIBitmap(ISWITCHCONTROL_2_ID, ISWITCHCONTROL_2_FN, kISwitchControl_2_N);
  pGraphics->AttachControl(new ISwitchControl(this, kISwitchControl_2_X, kISwitchControl_2_Y, kISwitchControl_2, &bitmap));

  // Attach the ISwitchControl 2 image "switch"
  bitmap = pGraphics->LoadIBitmap(ISWITCHCONTROL_3_ID, ISWITCHCONTROL_3_FN, kISwitchControl_3_N);
  pGraphics->AttachControl(new ISwitchControl(this, kISwitchControl_3_X, kISwitchControl_3_Y, kISwitchControl_3, &bitmap));


  // Attach the IInvisibleSwitchControl target area (IRECT expects the top left, then the bottom right coordinates)
  pGraphics->AttachControl(new IInvisibleSwitchControl(this, IRECT(kIInvisibleSwitchControl_X, kIInvisibleSwitchControl_Y, kIInvisibleSwitchControl_X + kIISC_W, kIInvisibleSwitchControl_Y + kIISC_H), kIInvisibleSwitchControl));

  // Attach a graphic for the InvisibleSwitchControl indicator, not associated with any parameter,
  // which we keep an index for, so we can push updates from the plugin class.

  bitmap = pGraphics->LoadIBitmap(IRADIOBUTTONSCONTROL_ID, IRADIOBUTTONSCONTROL_FN, kIRadioButtonsControl_N);

  mIISC_Indicator = pGraphics->AttachControl(new IBitmapControl(this, kIISC_I_X, kIISC_I_Y, &bitmap));

  //Attach the horizontal IRadioButtonsControl
  bitmap = pGraphics->LoadIBitmap(IRADIOBUTTONSCONTROL_ID, IRADIOBUTTONSCONTROL_FN, kIRadioButtonsControl_N);
  pGraphics->AttachControl(new IRadioButtonsControl(this, IRECT(kIRadioButtonsControl_H_X, kIRadioButtonsControl_H_Y, kIRadioButtonsControl_H_X + ((kIRBC_W*kIRBC_HN) + 10), kIRadioButtonsControl_H_Y + (kIRBC_H*kIRBC_HN)), kIRadioButtonsControl_H, kIRBC_HN, &bitmap, kHorizontal)); // added 10 to button area to show spreading the buttons out a bit

                                                                                                                                                                                                                                                                                         //Attach the vertical IRadioButtonsControl
  bitmap = pGraphics->LoadIBitmap(IRADIOBUTTONSCONTROL_ID, IRADIOBUTTONSCONTROL_FN, kIRadioButtonsControl_N);
  pGraphics->AttachControl(new IRadioButtonsControl(this, IRECT(kIRadioButtonsControl_V_X, kIRadioButtonsControl_V_Y, kIRadioButtonsControl_V_X + (kIRBC_W*kIRBC_VN), kIRadioButtonsControl_V_Y + (kIRBC_H*kIRBC_VN)), kIRadioButtonsControl_V, kIRBC_VN, &bitmap));


  //Attach the IContactControl
  bitmap = pGraphics->LoadIBitmap(ICONTACTCONTROL_ID, ICONTACTCONTROL_FN, kIContactControl_N);
  pGraphics->AttachControl(new IContactControl(this, kIContactControl_X, kIContactControl_Y, kIContactControl, &bitmap));

  //Attach the horizontal IFaderControl
  bitmap = pGraphics->LoadIBitmap(IFADERCONTROL_HORIZ_ID, IFADERCONTROL_HORIZ_FN);
  pGraphics->AttachControl(new IFaderControl(this, kIFaderControl_Horiz_X, kIFaderControl_Horiz_Y, kIFaderControl_Horiz_L, kIFaderControl_Horiz, &bitmap, kHorizontal));

  //Attach the vertical IFaderControl
  bitmap = pGraphics->LoadIBitmap(IFADERCONTROL_VERT_ID, IFADERCONTROL_VERT_FN);
  pGraphics->AttachControl(new IFaderControl(this, kIFaderControl_Vert_X, kIFaderControl_Vert_Y, kIFaderControl_Vert_L, kIFaderControl_Vert, &bitmap)); // kVertical is default

  IColor lineColor = IColor(255, 0, 0, 128);

  //Attach the default IKnobLineControl
  pGraphics->AttachControl(new IKnobLineControl(this, IRECT(kIKnobLineControl_def_X, kIKnobLineControl_def_Y, kIKnobLineControl_def_X + kIKLC_W, kIKnobLineControl_def_Y + kIKLC_H), kIKnobLineControl_def, &lineColor));

  //Attach the low geared IKnobLineControl
  pGraphics->AttachControl(new IKnobLineControl(this, IRECT(kIKnobLineControl_lo_gear_X, kIKnobLineControl_lo_gear_Y, kIKnobLineControl_lo_gear_X + kIKLC_W, kIKnobLineControl_lo_gear_Y + kIKLC_H), kIKnobLineControl_lo_gear, &lineColor, 0., 0., (-0.75*PI), (0.75*PI), kVertical, 2.0));

  //Attach the IKnobRotaterControl default rotation range
  bitmap = pGraphics->LoadIBitmap(IKNOBROTATERCONTROL_ID, IKNOBROTATERCONTROL_FN);
  pGraphics->AttachControl(new IKnobRotaterControl(this, kIKnobRotaterControl_def_X, kIKnobRotaterControl_def_Y, kIKnobRotaterControl_def, &bitmap));

  //Attach the IKnobRotaterControl restricted rotation range
  bitmap = pGraphics->LoadIBitmap(IKNOBROTATERCONTROL_ID, IKNOBROTATERCONTROL_FN);
  pGraphics->AttachControl(new IKnobRotaterControl(this, kIKnobRotaterControl_restrict_X, kIKnobRotaterControl_restrict_Y, kIKnobRotaterControl_restrict, &bitmap, -0.5*PI, 1.0*PI)); //restrict range going anticlockwise to 90 degrees and let clockwise movement go full range

                                                                                                                                                                                      //Attach the default IKnobMultiControl
  bitmap = pGraphics->LoadIBitmap(IKNOBMULTICONTROL_ID, IKNOBMULTICONTROL_FN, kIKnobMultiControl_N);
  pGraphics->AttachControl(new IKnobMultiControl(this, kIKnobMultiControl_def_X, kIKnobMultiControl_def_Y, kIKnobMultiControl_def, &bitmap));

  //Attach the horizontally moused IKnobMultiControl
  bitmap = pGraphics->LoadIBitmap(IKNOBMULTICONTROL_ID, IKNOBMULTICONTROL_FN, kIKnobMultiControl_N);
  pGraphics->AttachControl(new IKnobMultiControl(this, kIKnobMultiControl_Horiz_X, kIKnobMultiControl_Horiz_Y, kIKnobMultiControl_Horiz, &bitmap, kHorizontal));

  //Attach the IKnobRotatingMaskControl (specify the min and max angle in degrees, not radians, in this control)
  IBitmap base = pGraphics->LoadIBitmap(IKRMC_BASE_ID, IKRMC_BASE_FN);
  IBitmap mask = pGraphics->LoadIBitmap(IKRMC_MASK_ID, IKRMC_MASK_FN);
  IBitmap top = pGraphics->LoadIBitmap(IKRMC_TOP_ID, IKRMC_TOP_FN);
  pGraphics->AttachControl(new IKnobRotatingMaskControl(this, kIKRMC_X, kIKRMC_Y, kIKnobRotatingMaskControl, &base, &mask, &top, -135., 135.));

  //Attach IBitmapOverlayControl
  bitmap = pGraphics->LoadIBitmap(IBOC_ID, IBOC_FN);
  pGraphics->AttachControl(new IBitmapOverlayControl(this, kIBOC_X, kIBOC_Y, &bitmap, IRECT(kIBOC_T_X, kIBOC_T_Y, (kIBOC_T_X + kIBOC_T_W), (kIBOC_T_Y + kIBOC_T_W))));

  IText text = IText(14);

  //Attach ITextControl
  pGraphics->AttachControl(new ITextControl(this, IRECT(kITC_X, kITC_Y, (kITC_X + kITC_W), (kITC_Y + kITC_H)), &text, "我是你大爷"));

  //Attach ICaptionControl
  pGraphics->AttachControl(new ICaptionControl(this, IRECT(kICC_X, kICC_Y, (kICC_X + kICC_W), (kICC_Y + kICC_H)), kICaptionControl, &text));

  //Attach IURLControl
  pGraphics->AttachControl(new IURLControl(this, IRECT(kIUC_X, kIUC_Y, (kIUC_X + kIUC_W), (kIUC_Y + kIUC_W)), "https://github.com/audio-plugins/wdl-ce/wiki"));
  AttachGraphics(pGraphics);
  
}


void IPlugDistortion::OnParamChange(int paramIdx)
{
  IMutexLock lock(this);

  switch (paramIdx)
  {
    case kDrive:
    {
      double value = GetParam(kDrive)->Value();
      mDrive = 1. + 15. * value;
      mGain = pow(2., value) / mDrive;
      break;
    }
  }
}


void IPlugDistortion::ProcessDoubleReplacing(double** inputs, double** outputs, int nFrames)
{
  bool isMono = !IsInChannelConnected(1);

  for (int i = 0; i < nFrames; ++i)
  {
    double sample = isMono ? inputs[0][i] : 0.5 * (inputs[0][i] + inputs[1][i]);
    double output;

    for (int j = 0; j < mOversampling; ++j)
    {
      // Upsample
      if (j > 0) sample = 0.;
      mUpsample.Process(sample, mAntiAlias.Coeffs());
      sample = (double)mOversampling * mUpsample.Output();

      // Distortion
      if (WDL_DENORMAL_OR_ZERO_DOUBLE_AGGRESSIVE(&sample))
        sample = 0.;
      else
        sample = mGain * (fast_tanh(mDC + mDrive * sample) - mDistortedDC);

      // Downsample
      mDownsample.Process(sample, mAntiAlias.Coeffs());
      if (j == 0) output = mDownsample.Output();
    }

    outputs[0][i] = outputs[1][i] = output;
  }
}
