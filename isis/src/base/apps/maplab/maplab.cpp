#include "Isis.h"
#include <iostream>
#include <sstream>
#include <QString>
#include "Pvl.h"
#include "Cube.h"
#include "History.h"
#include "Projection.h"
#include "ProjectionFactory.h"

using namespace Isis;
using namespace std;

void IsisMain() {
  // Access input parameters (user interface)
  UserInterface &ui = Application::GetUserInterface();

  // Open the input cube
  Cube cube;
  cube.open(ui.GetFileName("FROM"), "rw");

  //Get the map projection file provided by the user
  Pvl userMap;
  userMap.Read(ui.GetFileName("MAP"));
  PvlGroup &mapGrp = userMap.FindGroup("Mapping", Pvl::Traverse);

  // Error checking to ensure the map projection file provided contains
  // information pertaining to a target, body radius, and longitude direction
  if(!mapGrp.HasKeyword("TargetName")) {
    QString msg = "The given MAP [" + userMap.Name() +
                  "] does not have the TargetName keyword.";
    throw IException(IException::User, msg, _FILEINFO_);
  }
  else if(!mapGrp.HasKeyword("EquatorialRadius") ||
          !mapGrp.HasKeyword("PolarRadius")) {
    QString msg = "The given MAP [" + userMap.Name() +
                  "] does not have the EquatorialRadius and PolarRadius keywords.";
    throw IException(IException::User, msg, _FILEINFO_);
  }
  else if(!mapGrp.HasKeyword("LongitudeDomain")) {
    QString msg = "The given MAP [" + userMap.Name() +
                  "] does not have the LongitudeDomain keyword.";
    throw IException(IException::User, msg, _FILEINFO_);
  }


  // Get user entered option
  QString option = ui.GetString("COORDINATES");

  double x = 0.0;
  double y = 0.0;
  Projection * proj = ProjectionFactory::Create(userMap, false);
  if(option == "XY") {
    x = ui.GetDouble("X");
    y = ui.GetDouble("Y");
  }
  else if(option == "LATLON") {
    proj->SetGround(ui.GetDouble("LAT"), ui.GetDouble("LON"));
    x = proj->XCoord();
    y = proj->YCoord();
  }
  else {
    QString message = "Invalid option [" + option + "] for parameter COORDINATES";
    throw IException(IException::User, message, _FILEINFO_);
  }

  double res = 0.0;
  double scale = 0.0;

  if(mapGrp.HasKeyword("PixelResolution")) {
    double localRadius = proj->LocalRadius(proj->TrueScaleLatitude());
    res = mapGrp.FindKeyword("PixelResolution");
    scale = (2.0 * Isis::PI * localRadius) / (360.0 * res);
  }
  else if(mapGrp.HasKeyword("Scale")) {
    double localRadius = proj->LocalRadius(proj->TrueScaleLatitude());
    scale = mapGrp.FindKeyword("Scale");
    res = (2.0 * Isis::PI * localRadius) / (360.0 * scale);
  }
  else {
    QString msg = "The given MAP[" + userMap.Name() +
                 "] does not have the PixelResolution or Scale keywords.";
    throw IException(IException::User, msg, _FILEINFO_);
  }

  //Read in line and sample inputs
  double line = ui.GetDouble("LINE");
  double samp = ui.GetDouble("SAMPLE");
  x = x - res * (samp - 0.5);
  y = y + res * (line - 0.5);

  //add origen values to Mapping Group
  mapGrp.AddKeyword(PvlKeyword("UpperLeftCornerX", toString(x), "meters"), Pvl::Replace);
  mapGrp.AddKeyword(PvlKeyword("UpperLeftCornerY", toString(y), "meters"), Pvl::Replace);

  if(!mapGrp.HasKeyword("PixelResolution")) {
    mapGrp.AddKeyword(PvlKeyword("PixelResolution", toString(res), "meters"));
  }
  if(!mapGrp.HasKeyword("Scale")) {
    mapGrp.AddKeyword(PvlKeyword("Scale", toString(scale), "pixels/degree"));
  }


  // Output the mapping group used to the Gui session log
  Application::GuiLog(userMap);
  // Extract label from cube file
  Pvl *label = cube.label();
  PvlObject &o = label->FindObject("IsisCube");
  // Add Mapping Group to input cube
  if(o.HasGroup("Mapping")) {
    o.DeleteGroup("Mapping");
  }
  o.AddGroup(mapGrp);

  // keep track of change to labels in history
  History hist = History("IsisCube");
  try {
    cube.read(hist);
  }
  catch(IException &e) {
  }
  hist.AddEntry();
  cube.write(hist);

  cube.close();
}
