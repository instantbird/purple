/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://testing-common/appInfoUtils.jsm");

function run_test() {
  // Check that purplexpcom (and so libpurple) loaded correctly.
  do_check_eq(Components.classes["@instantbird.org/libpurple/core;1"],
              "@instantbird.org/libpurple/core;1");
  let pcs = Components.classes["@instantbird.org/libpurple/core;1"]
                      .getService(Components.interfaces.purpleICoreService);
  do_check_true(pcs != null);
  do_check_true(pcs.version.length > 0);

  // Check that after initializing the core service, we have at least one prpl.
  do_get_profile();
  // Having an implementation of nsIXULAppInfo is required for
  // core.init to work.
  XULAppInfo.init();

  let core = Components.classes["@mozilla.org/chat/core-service;1"]
                       .getService(Components.interfaces.imICoreService);
  core.init();

  do_check_true(core.getProtocols().hasMoreElements());

  try {
    for each (let prplId in ["aim", "msn", "jabber", "yahoo"]) {
      let id = "prpl-" + prplId;
      let proto = core.getProtocolById(id);
      do_check_true(proto instanceof Ci.prplIProtocol);
      do_check_eq(proto.id, id);
    }
  } finally {
    core.quit();
  }
}
