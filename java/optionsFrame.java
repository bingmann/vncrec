//
//  Copyright (C) 1997, 1998 Olivetti & Oracle Research Laboratory
//
//  This is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This software is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this software; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
//  USA.
//

//
// Options frame.
//
// This deals with all the options the user can play with.
// It sets the encodings array and some booleans.
//

import java.awt.*;

class optionsFrame extends Frame {

  static String[] names = {
    "Encoding",
    "Use CopyRect",
    "Mouse buttons 2 and 3",
    "Draw each pixel for raw rectangles",
    "Optimise CopyRect for",
    "Share desktop",
  };

  static String[][] values = {
    { "Raw", "RRE", "CoRRE", "Hextile" },
    { "Yes", "No" },
    { "Normal", "Reversed" },
    { "Yes", "No" },
    { "Speed", "Correctness" },
    { "No", "Yes" },
  };

  final int encodingIndex = 0, useCopyRectIndex = 1, mouseButtonIndex = 2,
    drawEachPixelIndex = 3, copyRectSpeedIndex = 4, shareDesktopIndex = 5;

  Label[] labels = new Label[names.length];
  Choice[] choices = new Choice[names.length];
  Button dismiss;
  vncviewer v;


  //
  // The actual data which other classes look at:
  //

  int[] encodings = new int[10];
  int nEncodings;

  boolean reverseMouseButtons2And3;

  boolean drawEachPixelForRawRects;

  boolean optimiseCopyRectForSpeed;

  boolean shareDesktop;


  //
  // Constructor.  Set up the labels and choices from the names and values
  // arrays.
  //

  optionsFrame(vncviewer v1) {
    super("VNC Options");

    v = v1;

    GridBagLayout gridbag = new GridBagLayout();
    setLayout(gridbag);

    GridBagConstraints gbc = new GridBagConstraints();
    gbc.fill = GridBagConstraints.BOTH;

    for (int i = 0; i < names.length; i++) {
      labels[i] = new Label(names[i]);
      gbc.gridwidth = 1;
      gridbag.setConstraints(labels[i],gbc);
      add(labels[i]);

      choices[i] = new Choice();
      gbc.gridwidth = GridBagConstraints.REMAINDER;
      gridbag.setConstraints(choices[i],gbc);
      add(choices[i]);

      for (int j = 0; j < values[i].length; j++) {
	choices[i].addItem(values[i][j]);
      }
    }

    choices[encodingIndex].select("Hextile");

    setEncodings();

    reverseMouseButtons2And3 = false;

    drawEachPixelForRawRects = true;

    optimiseCopyRectForSpeed = true;

    dismiss = new Button("Dismiss");
    gbc.gridwidth = GridBagConstraints.REMAINDER;
    gridbag.setConstraints(dismiss,gbc);
    add(dismiss);

    pack();
  }


  //
  // Disable shareDesktop option
  //

  void disableShareDesktop() {
    labels[shareDesktopIndex].disable();
    choices[shareDesktopIndex].disable();
  }

  //
  // setEncodings looks at the encoding and copyRect choices and sets the
  // encodings array appropriately.  It also calls the vncviewer's
  // setEncodings method to send a message to the RFB server if necessary.
  //

  void setEncodings() {
    nEncodings = 0;
    if (choices[useCopyRectIndex].getSelectedItem().equals("Yes")) {
      encodings[nEncodings++] = rfbProto.EncodingCopyRect;
    }

    int preferredEncoding = rfbProto.EncodingRaw;

    if (choices[encodingIndex].getSelectedItem().equals("RRE")) {
      preferredEncoding = rfbProto.EncodingRRE;
    } else if (choices[encodingIndex].getSelectedItem().equals("CoRRE")) {
      preferredEncoding = rfbProto.EncodingCoRRE;
    } else if (choices[encodingIndex].getSelectedItem().equals("Hextile")) {
      preferredEncoding = rfbProto.EncodingHextile;
    }

    if (preferredEncoding == rfbProto.EncodingRaw) {
      choices[drawEachPixelIndex].select("No");
      drawEachPixelForRawRects = false;
    }

    encodings[nEncodings++] = preferredEncoding;
    if (preferredEncoding != rfbProto.EncodingRRE) {
      encodings[nEncodings++] = rfbProto.EncodingRRE;
    }
    if (preferredEncoding != rfbProto.EncodingCoRRE) {
      encodings[nEncodings++] = rfbProto.EncodingCoRRE;
    }
    if (preferredEncoding != rfbProto.EncodingHextile) {
      encodings[nEncodings++] = rfbProto.EncodingHextile;
    }

    v.setEncodings();
  }


  //
  // Respond to an action i.e. choice or button press
  //

  public boolean action(Event evt, Object arg) {

    if (evt.target == dismiss) {
      hide();
      return true;

    } else if ((evt.target == choices[encodingIndex]) ||
	       (evt.target == choices[useCopyRectIndex])) {

      setEncodings();
      return true;

    } else if (evt.target == choices[mouseButtonIndex]) {

      reverseMouseButtons2And3
	= choices[mouseButtonIndex].getSelectedItem().equals("Reversed");
      return true;

    } else if (evt.target == choices[drawEachPixelIndex]) {

      drawEachPixelForRawRects
	= choices[drawEachPixelIndex].getSelectedItem().equals("Yes");
      return true;

    } else if (evt.target == choices[copyRectSpeedIndex]) {

      optimiseCopyRectForSpeed
	= (choices[copyRectSpeedIndex].getSelectedItem().equals("Speed"));
      return true;

    } else if (evt.target == choices[shareDesktopIndex]) {

      shareDesktop
	= (choices[shareDesktopIndex].getSelectedItem().equals("Yes"));
      return true;

    }
    return false;
  }
}
