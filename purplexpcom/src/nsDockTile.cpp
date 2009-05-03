/* -*- Mode: C++; tab-aWidth: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is WebRunner
 *
 * The Initial Developer of the Original Code is
 * Matthew Gertner.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Matthew Gertner <matthew.gertner@gmail.com> (Original author)
 *   Florian Queze <florian@instantbird.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Development of this Contribution was supported by Yahoo! Inc. */

#include <Carbon/Carbon.h>

#include "nsDockTile.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(nsDockTile, nsIDockBadgeService)

nsDockTile::nsDockTile()
{
}

nsDockTile::~nsDockTile()
{
}

#define NS_ENSURE_NOERR1(aErr, aName)                  \
  if (aErr != noErr) {                                  \
    NS_WARNING(aName" failed");                         \
    ::EndCGContextForApplicationDockTile(context);      \
                                                        \
    return NS_ERROR_FAILURE;                            \
  }

#define NS_ENSURE_NOERR2(aErr, aName)                  \
  if (aErr != noErr) {                                  \
    NS_WARNING(aName" failed");                         \
    ::ATSUDisposeStyle(style);                          \
    ::EndCGContextForApplicationDockTile(context);      \
                                                        \
    return NS_ERROR_FAILURE;                            \
  }

NS_IMETHODIMP nsDockTile::SetBadgeText(const nsAString& aBadgeText)
{
  if (aBadgeText.IsEmpty()) {
    RestoreApplicationDockTileImage();
    mBadgeText = aBadgeText;
    return NS_OK;
  }

  CGContextRef context = ::BeginCGContextForApplicationDockTile();

  // Draw a circle.
  ::CGContextBeginPath(context);
  ::CGContextAddArc(context, 95.0, 95.0, 25.0, 0.0, 2 * M_PI, true);
  ::CGContextClosePath(context);

  // use #2fc600 for the color.
  ::CGContextSetRGBFillColor(context, 0.184, 0.776, 0.0, 1);
  ::CGContextFillPath(context);

  ATSUStyle style;
  OSStatus err = ::ATSUCreateStyle(&style);
  NS_ENSURE_NOERR1(err, "ATSUCreateStyle");

  Fixed size = Long2Fix(24);
  RGBColor white = { 0xFFFF, 0xFFFF, 0xFFFF };
  ATSUAttributeValuePtr values[2] = { &size, &white };
  ATSUAttributeTag tags[2] = { kATSUSizeTag, kATSUColorTag };
  ByteCount valueSizes[2] = { sizeof(Fixed), sizeof(RGBColor) };
  err = ::ATSUSetAttributes(style, 2, tags, valueSizes, values);
  NS_ENSURE_NOERR2(err, "ATSUSetAttributes");

  nsString text(aBadgeText);
  UniCharCount runLengths = kATSUToTextEnd;
  ATSUTextLayout textLayout;
  err = ::ATSUCreateTextLayoutWithTextPtr(text.get(),
                                          kATSUFromTextBeginning,
                                          kATSUToTextEnd, aBadgeText.Length(), 1,
                                          &runLengths, &style, &textLayout);
  NS_ENSURE_NOERR2(err, "ATSUCreateTextLayoutWithTextPtr");

  ATSUAttributeTag layoutTags[1] = { kATSUCGContextTag };
  ByteCount layoutValueSizes[1] = { sizeof(CGContextRef) };
  ATSUAttributeValuePtr layoutValues[1] = { &context };
  err = ::ATSUSetLayoutControls(textLayout, 1, layoutTags, layoutValueSizes,
                                layoutValues);
  NS_ENSURE_NOERR2(err, "ATSUSetLayoutControls");

  Rect boundingBox;
  err = ::ATSUMeasureTextImage(textLayout, kATSUFromTextBeginning,
                               kATSUToTextEnd, Long2Fix(0), Long2Fix(0),
                               &boundingBox);
  NS_ENSURE_NOERR2(err, "ATSUMeasureTextImage");

  // Center text inside circle
  err = ::ATSUDrawText(textLayout, kATSUFromTextBeginning, kATSUToTextEnd,
                       Long2Fix(92 - (boundingBox.right - boundingBox.left) / 2),
                       Long2Fix(95 - (boundingBox.bottom - boundingBox.top) / 2));
  NS_ENSURE_NOERR2(err, "ATSUDrawText");

  ::ATSUDisposeStyle(style);
  ::ATSUDisposeTextLayout(textLayout);
  ::CGContextFlush(context);

  mBadgeText = aBadgeText;
  return NS_OK;
}

NS_IMETHODIMP nsDockTile::GetBadgeText(nsAString& aBadgeText)
{
  aBadgeText = mBadgeText;
  return NS_OK;
}
