const Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

var XULAppInfo = {
  vendor: "Mozilla",
  name: "XPCShell",
  ID: "{39885e5f-f6b4-4e2a-87e5-6259ecf79011}",
  version: "5",
  appBuildID: "2007010101",
  platformVersion: "1.9",
  platformBuildID: "2007010101",
  inSafeMode: false,
  logConsoleErrors: true,
  OS: "XPCShell",
  XPCOMABI: "noarch-spidermonkey",

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIXULAppInfo, Ci.nsIXULRuntime,
					 Ci.nsISupports])
};

var XULAppInfoFactory = {
  createInstance: function (outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return XULAppInfo.QueryInterface(iid);
  }
};

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
  Components.manager.QueryInterface(Ci.nsIComponentRegistrar)
            .registerFactory(Components.ID("{48a4e946-1f9f-4224-b4b0-9a54183cb81e}"),
			     "XULAppInfo", "@mozilla.org/xre/app-info;1",
			     XULAppInfoFactory);

  let core = Components.classes["@mozilla.org/chat/core-service;1"]
                       .getService(Components.interfaces.imICoreService);
  core.init();
  do_check_true(core.getProtocols().hasMoreElements());

  core.quit();
}
