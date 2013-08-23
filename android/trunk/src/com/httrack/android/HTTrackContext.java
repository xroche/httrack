package com.httrack.android;

import java.io.File;

public class HTTrackContext {
  // Project path
  protected File projectPath;
  protected File rscPath;

  // Options mapper, containing all options
  protected final OptionsMapper mapper = new OptionsMapper();

  public File getProjectPath() {
    return projectPath;
  }

  public void setProjectPath(File projectPath) {
    this.projectPath = projectPath;
  }

  public File getRscPath() {
    return rscPath;
  }

  public void setRscPath(File rscPath) {
    this.rscPath = rscPath;
  }

  public OptionsMapper getMapper() {
    return mapper;
  }
}
