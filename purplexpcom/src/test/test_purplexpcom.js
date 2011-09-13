function run_test() {
  do_check_eq(Components.classes["@instantbird.org/purple/core;1"],
              "@instantbird.org/purple/core;1");
  let pcs = Components.classes["@instantbird.org/purple/core;1"]
                      .getService(Components.interfaces.purpleICoreService);
  do_check_true(pcs != null);

  do_get_profile();
  pcs.init();

  do_check_true(pcs.getProtocols().hasMoreElements());

  pcs.quit();
}
