/*
 ==============================================================================
 
 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers. 
 
 See LICENSE.txt for  more info.
 
 ==============================================================================
*/

#pragma once

/**
 * @file
 * @copydoc IEditorDelegate
 */

#include <cassert>
#include <cstring>
#include <stdint.h>

#include "ptrlist.h"

#include "IPlugParameter.h"
#include "IPlugMidi.h"
#include "IPlugStructs.h"

/** This pure virtual interface delegates communication in both directions between a UI editor and something else (which is usually a plug-in)
 *  It is also the class that owns parameter objects (for historical reasons) - although it's not necessary to allocate them
 *
 *  This is the lowest level base class in iPlug 2 that facilitates distributing editor and DSP parts for plug-in formats that need that, but also allowing non-distributed plug-ins to use the same API.
 *  In distributed plug-in architectures certain methods will be overridden in order to pipe messages to various places, using whatever mechanism that plug-in format requires.
 *  In this case, there are actually two classes that implement the IEditorDelegate interface, but only one which is directly connected to the user interface (IGraphics etc.),
 *  the other being connected to a class inheriting IPlugAPIBase that deals with processing audio, see for example IPlugVST3Processor
 *
 *  Note on method names:
 *  - "FromUI" in a method name, means that that method is called by something in the UI i.e. when the user interacts with a control.
 *  - "FromDelegate" in a method name means that method is called from a class that implements the IEditorDelegate interface,
 *     which is usually your plug-in base class, but may not be in the case of an isolated editor class, or if you are using IGraphics without IPlug, and your IEditorDelegate is not a plug-in
 *
 *  NOTES:
 *  A parameter VALUE is a floating point number linked to an integer parameter index. TODO: Normalised ?
 *  A parameter OBJECT (IParam) is an instance of the IParam class as defined in IPlugParameter.h
 *  A parameter OBJECT is also referred to as a "param", in method names such as IEditorDelegate::GetParam(int paramIdx) and IControl::GetParam(). */
class IEditorDelegate
{
public:
  IEditorDelegate(int nParams)
  {
    for (int i = 0; i < nParams; i++)
      AddParam();
  }
  
  virtual ~IEditorDelegate()
  {
    mParams.Empty(true);
  }
  
  /** Adds an IParam to the parameters ptr list
   * Note: This is only used in special circumstances, since most plug-in formats don't support dynamic parameters
   * @return Ptr to the newly created IParam object */
  IParam* AddParam() { return mParams.Add(new IParam()); }
  
  /** Remove an IParam at a particular index
   * Note: This is only used in special circumstances, since most plug-in formats don't support dynamic parameters
   * @param idx The index of the parameter to remove */
  void RemoveParam(int idx) { return mParams.Delete(idx); }
  
  /** Get a pointer to one of the delegate's IParam objects
   * @param paramIdx The index of the parameter object to be got
   * @return A pointer to the IParam object at paramIdx or nullptr if paramIdx is invalid */
  IParam* GetParam(int paramIdx) { return mParams.Get(paramIdx); }
    
  /** Get a const pointer to one of the delegate's IParam objects (for const methods)
   * @param paramIdx The index of the parameter object to be got
   * @return A pointer to the IParam object at paramIdx or nullptr if paramIdx is invalid */
  const IParam* GetParam(int paramIdx) const { return mParams.Get(paramIdx); }

  /** @return Returns the number of parameters that belong to the plug-in. */
  int NParams() const { return mParams.GetSize(); }
  
  /** Loops through all parameters, calling SendParameterValueFromDelegate() with the current value of the parameter
   *  This is important when modifying groups of parameters, restoring state and opening the UI, in order to update it with the latest values*/
  void SendCurrentParamValuesFromDelegate()
  {
    for (int i = 0; i < NParams(); ++i)
    {
      SendParameterValueFromDelegate(i, GetParam(i)->GetNormalized(), true);
    }
  }
  
  /** If you are not using IGraphics, you can implement this method to attach to the native parent view e.g. NSView, UIView, HWND.
   *  Defer calling OnUIOpen() if nessecary. */
  virtual void* OpenWindow(void* pParent) { OnUIOpen(); return nullptr; }
  
  /** If you are not using IGraphics you can if you need to free resources etc when the window closes. Call base implementation. */
  virtual void CloseWindow() { OnUIClose(); }
  
#pragma mark - Methods you may want to override...
  /** Override this method to do something before the UI is opened. Call base implementation. */
  virtual void OnUIOpen() { SendCurrentParamValuesFromDelegate(); }
  
  /** Override this method to do something before the UI is closed. */
  virtual void OnUIClose() {};
  
  /** This is an OnParamChange that will only trigger on the UI thread at low priority, and therefore is appropriate for hiding or showing elements of the UI.
   * You should not update parameter objects using this method.
   * @param paramIdx The index of the parameter that changed */
  virtual void OnParamChangeUI(int paramIdx, EParamSource source = kUnknown) {};
  
  /** Handle incoming MIDI messages sent to the user interface
   * @param msg The MIDI message to process  */
  virtual void OnMidiMsgUI(const IMidiMsg& msg) {};
  
  /** Handle incoming SysEx messages sent to the user interface
   * @param msg The SysEx message to process */
  virtual void OnSysexMsgUI(const ISysEx& msg) {};
  
  /** This could be implemented in either DSP or EDITOR to receive a message from the other one */
  virtual bool OnMessage(int messageTag, int controlTag, int dataSize, const void* pData) { return false; }
  
  /** This is called by API classes after restoring state and by IPluginBase::RestorePreset(). Typically used to update user interface, where multiple parameter values have changed.
   * If you need to do something when state is restored you can override it
   * If you override this method you should call this parent, or implement the same functionality in order to get controls to update, when state is restored. */
  virtual void OnRestoreState() { SendCurrentParamValuesFromDelegate(); };
  
#pragma mark - Methods for sending values TO the user interface
  /** SendControlValueFromDelegate (Abbreviation: SCVFD)
   * WARNING: should not be called on the realtime audio thread.
   * In IGraphics plug-ins, this method is used to update controls in the user interface from a class implementing IEditorDelegate, when the control is not linked to a parameter.
   * A typical use case would be a meter control.
   * In OnIdle() your plug-in would call this method to update the IControl's value.
   * @param controlTag A tag for the control
   * @param normalizedValue The normalised value to set the control to. This will modify IControl::mValue; */
  virtual void SendControlValueFromDelegate(int controlTag, double normalizedValue) {};
  
  /** SendControlMsgFromDelegate (Abbreviation: SCMFD)
   * WARNING: should not be called on the realtime audio thread.
   * This method can be used to send opaque data from a class implementing IEditorDelegate to a specific control in the user interface.
   * The message can be handled in the destination control via IControl::OnMsgFromDelegate
   * @param controlTag A unique tag to identify the control that is the destination of the message
   * @param messageTag A unique tag to identify the message
   * @param dataSize The size in bytes of the data payload pointed to by pData. Note: if this is nonzero, pData must be valid.
   * @param pData Ptr to the opaque data payload for the message */
  virtual void SendControlMsgFromDelegate(int controlTag, int messageTag, int dataSize = 0, const void* pData = nullptr) { OnMessage(messageTag, controlTag, dataSize, pData); }
  
  /** SendArbitraryMsgFromDelegate (Abbreviation: SAMFD)
   * WARNING: should not be called on the realtime audio thread.
   * This method can be used to send opaque data from a class implementing IEditorDelegate to the IEditorDelegate connected to the user interface
   * The message can be handled at the destination via IEditorDelegate::OnMessage()
   * @param messageTag A unique tag to identify the message
   * @param dataSize The size in bytes of the data payload pointed to by pData. Note: if this is nonzero, pData must be valid.
   * @param pData Ptr to the opaque data payload for the message */
  virtual void SendArbitraryMsgFromDelegate(int messageTag, int dataSize = 0, const void* pData = nullptr) { OnMessage(messageTag, kNoTag, dataSize, pData); }
  
  /** SendMidiMsgFromDelegate (Abbreviation: SMMFD)
   * WARNING: should not be called on the realtime audio thread.
   * This method can be used to send regular MIDI data from the class implementing IEditorDelegate to the user interface
   * The message can be handled at the destination via IEditorDelegate::OnMidiMsgUI()
   * @param msg an IMidiMsg Containing the MIDI message to send to the user interface. */
  virtual void SendMidiMsgFromDelegate(const IMidiMsg& msg) { OnMidiMsgUI(msg); }
  
  /** SendSysexMsgFromDelegate (Abbreviation: SSMFD)
   * WARNING: should not be called on the realtime audio thread.
   * This method can be used to send SysEx data from the class implementing IEditorDelegate to the user interface
   * The message can be handled at the destination via IEditorDelegate::OnSysexMsgUI()
   * @param msg an ISysEx Containing the SysEx data to send to the user interface. */
  virtual void SendSysexMsgFromDelegate(const ISysEx& msg) { OnSysexMsgUI(msg); }
  
  /** SendParameterValueFromDelegate (Abbreviation: SPVFD)
   * WARNING: should not be called on the realtime audio thread.
   * This method is called by the class implementing the delegate interface (not the plug-in API class) in order to update the user interface with the new parameter values, typically after automation.
   * The similarly named IPlugAPIBase::SendParameterValueFromAPI() should take care of queueing and deferring, if there is no main thread notification from the API
   * If you override this method you should call the base class implementation to make sure OnParamChangeUI gets triggered
   * In IGraphics plug-ins, this will update any IControls that have their mParamIdx set > -1
   * @param paramIdx The index of the parameter to be updated
   * @param value The new value of the parameter
   * @param normalized \c true if value is normalised */
  virtual void SendParameterValueFromDelegate(int paramIdx, double value, bool normalized) { OnParamChangeUI(paramIdx, EParamSource::kDelegate); } // TODO: normalised?

#pragma mark - Methods for sending values FROM the user interface
  // The following methods are called from the user interface in order to set or query values of parameters in the class implementing IEditorDelegate
  
  /** Called by the UI at the beginning of a parameter change gesture, in order to notify the host
   * (via a call in the API class) that the parameter is going to be modified
   * The host may be trying to automate the parameter as well, so it needs to relinquish control when the user is modifying something in the user interface.
   * @param paramIdx The index of the parameter that is changing value */
  virtual void BeginInformHostOfParamChangeFromUI(int paramIdx) = 0;
  
  /** SPVFUI Called by the UI during a parameter change gesture, in order to notify the host of the new value (via a call in the API class)
   * If you override this method you should call the base class implementation to make sure OnParamChangeUI gets triggered
   * @param paramIdx The index of the parameter that is changing value
   * @param normalizedValue The new normalised value of the parameter */
  virtual void SendParameterValueFromUI(int paramIdx, double normalizedValue)
  {
    assert(paramIdx < NParams());
    
    GetParam(paramIdx)->SetNormalized(normalizedValue);
    OnParamChangeUI(paramIdx, EParamSource::kUI);
  }
  
  /** Called by the user interface at the end of a parameter change gesture, in order to notify the host
   * (via a call in the API class) that the parameter is no longer being modified
   * The host may be trying to automate the parameter as well, so it needs to relinquish control when the user is
   * modifying something in the user interface.
   * @param paramIdx The index of the parameter that is changing value */
  virtual void EndInformHostOfParamChangeFromUI(int paramIdx) = 0;
  
  /** When modifying a range of parameters in the editor, it can be necessary to broadcast that fact, for instance in a distributed plug-in.
   *  You can use it if you restore a preset using a custom preset mechanism. */
  virtual void DirtyParametersFromUI() {};
  
  /** If the editor changes UI dimensions we need to call into the plug-in API to  resize the window in the plugin
   * returns a bool to indicate whether the DAW or plugin class has resized the host window */
  virtual bool EditorResizeFromUI(int viewWidth, int viewHeight) { return false; }
    
  /** If the editor changes arbitrary data (such as layout/scale) this is called to store data into the plugin*/
  virtual void EditorDataChangedFromUI(const IByteChunk& data) {}
  
  /** SendMidiMsgFromUI (Abbreviation: SMMFUI)
   * This method should be used  when  sending a MIDI message from the UI. For example clicking on a key in a virtual keyboard.
   * Eventually the MIDI message can be handled in IPlugProcessor::ProcessMidiMsg(), from where it can be used to trigger sound and or forwarded to the API's MIDI output.
   * @param msg The MIDI message to send. */
  virtual void SendMidiMsgFromUI(const IMidiMsg& msg) {};

  /** SendMidiMsgFromUI (Abbreviation: SSMFUI)
   * If a plug-in can send Sysex data as a result of actions in the user interface, this method can be used.
   * Unlike SendMidiMsgFromUI, Sysex messages will not be received in IPlugProcessor::ProcessSysex()
   * Since it is extremely unlikely that you would want to use Sysex to communicate between editor and processor \todo is this correct?
   * @param msg The Sysex message to send. */
  virtual void SendSysexMsgFromUI(const ISysEx& msg) {};
  
  /** SendArbitraryMsgFromUI (Abbreviation: SAMFUI)
  * @param messageTag A unique tag to identify the message
  * @param controlTag A unique tag to identify the control that sent the message, if desired
  * @param dataSize The size in bytes of the data payload pointed to by pData. Note: if this is nonzero, pData must be valid.
  * @param pData Ptr to the opaque data payload for the message */
  virtual void SendArbitraryMsgFromUI(int messageTag, int controlTag = kNoTag, int dataSize = 0, const void* pData = nullptr) {};
#pragma mark -
  /** This method is needed, for remote editors to avoid a feedback loop */
  virtual void DeferMidiMsg(const IMidiMsg& msg) {};
  
  /** This method is needed, for remote editors to avoid a feedback loop */
  virtual void DeferSysexMsg(const ISysEx& msg) {};

  /** @return The width of the plug-in editor in pixels */
  int GetEditorWidth() const { return mEditorWidth; }
  
  /** @return The height of the plug-in editor in pixels */
  int GetEditorHeight() const { return mEditorHeight; }
  
  /** @return An IByteChunk with any arbitrary data that the editor wishes to store  */
  const IByteChunk& GetEditorData() const { return mEditorData; }
  
  /** This method should be called to set and unserialize editor data from the plugin
   * @param data A IByteChunk containing the new data
   * @param startPos Starting point in the chunk
   * @return The new chunk position (endPos)*/
  virtual int SetEditorData(const IByteChunk& data, int startPos) { return startPos; }

protected:
  /** The width of the plug-in editor in pixels. Can be updated by resizing, exists here for persistance, even if UI doesn't exist. */
  int mEditorWidth = 0;
  /** The height of the plug-in editor in pixels. Can be updated by resizing, exists here for persistance, even if UI doesn't exist */
  int mEditorHeight = 0;
  /** Any arbitrary data that the editor need to store (e.g. scale etc.) */
  IByteChunk mEditorData;
  /** A list of IParam objects. This list is populated in the delegate constructor depending on the number of parameters passed as an argument to IPLUG_CTOR in the plug-in class implementation constructor */
  WDL_PtrList<IParam> mParams;
};
