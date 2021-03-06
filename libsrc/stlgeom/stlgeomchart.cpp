//20.11.1999 third part of stlgeom.cc, functions with chart and atlas

#include <mystdlib.h>

#include <myadt.hpp>
#include <linalg.hpp>
#include <gprim.hpp>

#include <meshing.hpp>

#include "stlgeom.hpp"

namespace netgen
{

int chartdebug = 0;



void STLGeometry :: MakeAtlas(Mesh & mesh)
{
  int timer1 = NgProfiler::CreateTimer ("makeatlas");
  /*
  int timerb = NgProfiler::CreateTimer ("makeatlas - begin");
  int timere = NgProfiler::CreateTimer ("makeatlas - end");  
  int timere1 = NgProfiler::CreateTimer ("makeatlas - end1");  
  int timere2 = NgProfiler::CreateTimer ("makeatlas - end2");  
  int timer2 = NgProfiler::CreateTimer ("makeatlas - part 2");
  int timer3 = NgProfiler::CreateTimer ("makeatlas - part 3");
  int timer4 = NgProfiler::CreateTimer ("makeatlas - part 4");
  int timer4a = NgProfiler::CreateTimer ("makeatlas - part 4a");
  int timer4b = NgProfiler::CreateTimer ("makeatlas - part 4b");
  int timer4c = NgProfiler::CreateTimer ("makeatlas - part 4c");
  int timer4d = NgProfiler::CreateTimer ("makeatlas - part 4d");
  int timer4e = NgProfiler::CreateTimer ("makeatlas - part 4e");
  int timer5 = NgProfiler::CreateTimer ("makeatlas - part 5");
  int timer5a = NgProfiler::CreateTimer ("makeatlas - part 5a");
  int timer5b = NgProfiler::CreateTimer ("makeatlas - part 5b");
  int timer5cs = NgProfiler::CreateTimer ("makeatlas - part 5cs");
  int timer5cl = NgProfiler::CreateTimer ("makeatlas - part 5cl");
  */
  PushStatusF("Make Atlas");

  double h = mparam.maxh;
   
  double atlasminh = 5e-3 * Dist (boundingbox.PMin(), boundingbox.PMax());
  PrintMessage(5, "atlasminh = ", atlasminh);

  //speedup for make atlas
  if (GetNT() > 50000)
    mesh.SetGlobalH(min2 (0.05*Dist (boundingbox.PMin(), boundingbox.PMax()),
			  mparam.maxh));

  atlas.SetSize(0);
  ClearSpiralPoints();
  BuildSmoothEdges();
  
  // NgProfiler::StartTimer (timer1);

  double chartangle = stlparam.chartangle;
  double outerchartangle = stlparam.outerchartangle;

  chartangle = chartangle/180.*M_PI;
  outerchartangle = outerchartangle/180.*M_PI;

  double coschartangle = cos(chartangle);
  double cosouterchartangle = cos(outerchartangle);
  double cosouterchartanglehalf = cos(0.5*outerchartangle);
  double sinchartangle = sin(chartangle);
  double sinouterchartangle = sin(outerchartangle);

  Array<int> outermark(GetNT());     //marks all trigs form actual outer region
  Array<int> outertested(GetNT());   //marks tested trigs for outer region
  Array<int> pointstochart(GetNP()); //point in chart becomes chartnum
  Array<int> innerpointstochart(GetNP()); //point in chart becomes chartnum
  Array<int> chartpoints;             //point in chart becomes chartnum
  Array<int> innerchartpoints;
  Array<Point<3>> innerchartpts;
  Array<int> dirtycharttrigs;

  Array<int> chartdistacttrigs (GetNT());   //outercharttrigs
  chartdistacttrigs = 0;


  STLBoundary chartbound(this); //knows the actual chart boundary
  //int chartboundarydivisions = 10;
  markedsegs.SetSize(0); //for testing!!!

  Array<int> chartpointchecked(GetNP()); //for dirty-chart-trigs

  pointstochart.SetSize(GetNP());
  innerpointstochart.SetSize(GetNP());
  chartmark.SetSize(GetNT());

  for (int i = 1; i <= GetNP(); i++)
    {
      innerpointstochart.Elem(i) = 0;
      pointstochart.Elem(i) = 0;
      chartpointchecked.Elem(i) = 0;
    }

  double eps = 1e-12 * Dist (boundingbox.PMin(), boundingbox.PMax());

  int spiralcheckon = stldoctor.spiralcheck;
  if (!spiralcheckon) {PrintWarning("++++++++++++\nspiral deactivated by user!!!!\n+++++++++++++++"); }

  chartmark = 0;
  outermark = 0;
  outertested = 0;

  double atlasarea = Area();
  double workedarea = 0;
  double showinc = 100.*5000./(double)GetNT();
  double nextshow = 0;
  // Point<3> startp;
  int lastunmarked = 1;

  PrintMessage(5,"one dot per 5000 triangles: ");

  int markedtrigcnt = 0;
  while (markedtrigcnt < GetNT())
    {
      if (multithread.terminate) { PopStatus(); return; }

      // NgProfiler::StartTimer (timerb);      
      if (workedarea / atlasarea*100. >= nextshow) 
      	{PrintDot(); nextshow+=showinc;}

      SetThreadPercent(100.0 * workedarea / atlasarea);

      STLChart * chart = new STLChart(this);
      atlas.Append(chart);

      // *testout << "Chart " << atlas.Size() << endl;

      //find unmarked trig
      int prelastunmarked = lastunmarked;

      bool found = false;
      for (int j = lastunmarked; j <= GetNT(); j++)
	if (!GetMarker(j))
	  {
	    found = true;
	    lastunmarked = j;
	    break;
	  }

      chartpoints.SetSize(0);  
      innerchartpoints.SetSize(0);
      innerchartpts.SetSize(0);
      chartbound.Clear();
      chartbound.SetChart(chart);
      
      chartbound.BuildSearchTree();  // different !!!

      if (!found) { PrintSysError("Make Atlas, no starttrig found"); return; }

      //find surrounding trigs
      // int starttrig = j;
      int starttrig = lastunmarked;

      Point<3> startp = GetPoint(GetTriangle(starttrig).PNum(1));

      int accepted;
      int chartnum = GetNOCharts();
	  
      Vec<3> sn = GetTriangle(starttrig).Normal();
      chart->SetNormal (startp, sn);
      
      // *testout << "first trig " << starttrig << ", n = " << sn << endl;

      SetMarker(starttrig, chartnum);
      markedtrigcnt++;
      chart->AddChartTrig(starttrig);
      chartbound.AddTriangle(GetTriangle(starttrig));

      workedarea += GetTriangle(starttrig).Area(points);

      for (int i = 1; i <= 3; i++)
	{	      
	  innerpointstochart.Elem(GetTriangle(starttrig).PNum(i)) = chartnum;
	  pointstochart.Elem(GetTriangle(starttrig).PNum(i)) = chartnum;
	  chartpoints.Append(GetTriangle(starttrig).PNum(i));
	  innerchartpoints.Append(GetTriangle(starttrig).PNum(i));
	}

      bool changed = true;
      int oldstartic = 1;
      int oldstartic2;
      // NgProfiler::StopTimer (timerb);      
      // NgProfiler::StartTimer (timer2);


      while (changed)
	{   
	  changed = false;
	  oldstartic2 = oldstartic;
	  oldstartic = chart->GetNT();
	  //	      for (ic = oldstartic2; ic <= chart->GetNT(); ic++)
	  for (int ic = oldstartic2; ic <= oldstartic; ic++)
	    {
	      int i = chart->GetTrig(ic);
	      if (GetMarker(i) == chartnum)
		{
		  for (int j = 1; j <= NONeighbourTrigs(i); j++)
		    {
		      int nt = NeighbourTrig(i,j);
                      // *testout << "check trig " << nt << endl;
		      int np1, np2;
		      GetTriangle(i).GetNeighbourPoints(GetTriangle(nt),np1,np2);
		      if (GetMarker(nt) == 0 && !IsEdge(np1,np2))
			{
			  Vec<3> n2 = GetTriangle(nt).Normal();
                          // *testout << "acos = " << 180/M_PI*acos (n2*sn) << endl;
			  if ( (n2 * sn) >= coschartangle )
			    {
                              // *testout << "good angle " << endl;
			      accepted = 1;
			      /*
				//alter spiralentest, schnell, aber ungenau
			      for (k = 1; k <= 3; k++)
				{
				  //find overlapping charts:
				  Point3d pt = GetPoint(GetTriangle(nt).PNum(k));
				  if (innerpointstochart.Get(GetTriangle(nt).PNum(k)) != chartnum)
				    {
				      for (l = 1; l <= chartpoints.Size(); l++)
					{
					  Vec3d vptpl(GetPoint(chartpoints.Get(l)), pt);
					  double vlen = vptpl.Length();
					  if (vlen > 0)
					    {
					      vptpl /= vlen;
					      if ( fabs( vptpl * sn) > sinchartangle )
						{
						  accepted = 0;
						  break;
						}
					    } 
					}

				    }
				}
			      */
			      
			      //find overlapping charts exacter (fast, too): 
			      for (int k = 1; k <= 3; k++) 
				{ 
				  int nnt = NeighbourTrig(nt,k);
				  if (GetMarker(nnt) != chartnum)
				    {
				      int nnp1, nnp2; 
				      GetTriangle(nt).GetNeighbourPoints(GetTriangle(nnt),nnp1,nnp2);

				      accepted = chartbound.TestSeg(GetPoint(nnp1),
								    GetPoint(nnp2),
								    sn,sinchartangle,1 /*chartboundarydivisions*/ ,points, eps);
                                      
                                      // if (!accepted) *testout << "not acc due to testseg" << endl;

				      Vec<3> n3 = GetTriangle(nnt).Normal();
				      if ( (n3 * sn) >= coschartangle  &&
					   IsSmoothEdge (nnp1, nnp2) )
					accepted = 1;
				    }
				  if (!accepted) 
                                    break;
				}
			      
                              
			      if (accepted)
				{
                                  // *testout << "trig accepted" << endl;
				  SetMarker(nt, chartnum); 
				  changed = true;
				  markedtrigcnt++;
				  workedarea += GetTriangle(nt).Area(points);
				  chart->AddChartTrig(nt);

				  chartbound.AddTriangle(GetTriangle(nt));

				  for (int k = 1; k <= 3; k++)
				    {
				      if (innerpointstochart.Get(GetTriangle(nt).PNum(k))
					  != chartnum) 
					{
					  innerpointstochart.Elem(GetTriangle(nt).PNum(k)) = chartnum;
					  pointstochart.Elem(GetTriangle(nt).PNum(k)) = chartnum;
					  chartpoints.Append(GetTriangle(nt).PNum(k));
					  innerchartpoints.Append(GetTriangle(nt).PNum(k));
					}
				    }
				}
			    }	       
			}
		    }
		}
	    }
	}

      innerchartpts.SetSize(innerchartpoints.Size());
      for (size_t i = 0; i < innerchartpoints.Size(); i++)
        innerchartpts[i] = GetPoint(innerchartpoints[i]);
      // chartbound.BuildSearchTree();  // different !!!
      
      // NgProfiler::StopTimer (timer2);
      // NgProfiler::StartTimer (timer3);
      
      //find outertrigs

      //      chartbound.Clear(); 
      // warum, ic-bound auf edge macht Probleme js ???


      outermark.Elem(starttrig) = chartnum;
      //chart->AddOuterTrig(starttrig);
      changed = true;
      oldstartic = 1;
      while (changed)
	{   
	  changed = false;
	  oldstartic2 = oldstartic;
	  oldstartic = chart->GetNT();

	  for (int ic = oldstartic2; ic <= oldstartic; ic++)
	    {
	      int i = chart->GetTrig(ic);
	      if (outermark.Get(i) != chartnum) continue;
	      
	      for (int j = 1; j <= NONeighbourTrigs(i); j++)
		{
		  int nt = NeighbourTrig(i,j);
		  if (outermark.Get(nt) == chartnum) continue;
		  
		  const STLTriangle & ntrig = GetTriangle(nt);
		  int np1, np2;
		  GetTriangle(i).GetNeighbourPoints(GetTriangle(nt),np1,np2);
		  
		  if (IsEdge (np1, np2)) continue;
		  
		  
		  /*
		    if (outertested.Get(nt) == chartnum)
		    continue;
		  */
		  outertested.Elem(nt) = chartnum;
		  
		  Vec<3> n2 = GetTriangle(nt).Normal();

		  //abfragen, ob noch im tolerierten Winkel
		  if ( (n2 * sn) >= cosouterchartangle )
		    {
		      accepted = 1;
		      
                      // NgProfiler::StartTimer (timer4);
		      bool isdirtytrig = false;
		      Vec<3> gn = GetTriangle(nt).GeomNormal(points);
		      double gnlen = gn.Length();
		      
		      if (n2 * gn <= cosouterchartanglehalf * gnlen)
			isdirtytrig = true;
		      
		      //zurueckweisen, falls eine Spiralartige outerchart entsteht
		      
		      //find overlapping charts exacter: 
		      //do not check dirty trigs!
                      // NgProfiler::StartTimer (timer4a);
                      
		      if (spiralcheckon && !isdirtytrig)
			for (int k = 1; k <= 3; k++) 
			  {
                            // NgProfiler::StartTimer (timer4b);                            
			    int nnt = NeighbourTrig(nt,k);
			    
			    if (outermark.Elem(nnt) != chartnum)
			      {
                                // NgProfiler::StartTimer (timer4c);
				int nnp1, nnp2; 
				GetTriangle(nt).GetNeighbourPoints(GetTriangle(nnt),nnp1,nnp2);
                                // NgProfiler::StopTimer (timer4c);
				
                                // NgProfiler::StartTimer (timer4d);

				accepted = 
				  chartbound.TestSeg(GetPoint(nnp1),GetPoint(nnp2),
						     sn,sinouterchartangle, 0 /*chartboundarydivisions*/ ,points, eps);
				
                                // NgProfiler::StopTimer (timer4d);				

                                // NgProfiler::StartTimer (timer4e);

				Vec<3> n3 = GetTriangle(nnt).Normal();
				if ( (n3 * sn) >= cosouterchartangle  &&
				     IsSmoothEdge (nnp1, nnp2) )
				  accepted = 1;
                                // NgProfiler::StopTimer (timer4e);                                
			      }
                            // NgProfiler::StopTimer (timer4b);                            
			    if (!accepted) break;
			  }
                      // NgProfiler::StopTimer (timer4a);
		      
                      //  NgProfiler::StopTimer (timer4);		      

                      // NgProfiler::RegionTimer reg5(timer5);		      

                      
		      // outer chart is only small environment of
		      //    inner chart:
		      
		      if (accepted)
			{
                          // NgProfiler::StartTimer (timer5a);
			  accepted = 0;
			  
			  for (int k = 1; k <= 3; k++)
			    if (innerpointstochart.Get(ntrig.PNum(k)) == chartnum)
			      {
				accepted = 1; 
				break;
			      }

                          // NgProfiler::StopTimer (timer5a);
                          // int timer5csl = (innerchartpts.Size() < 100) ? timer5cs : timer5cl;
                          // NgProfiler::StartTimer (timer5csl);
			  
			  if (!accepted)
			    for (int k = 1; k <= 3; k++)
			      {
				Point<3> pt = GetPoint(ntrig.PNum(k));					  
				double h2 = sqr(mesh.GetH(pt));
                                /*
                                for (int l = 1; l <= innerchartpoints.Size(); l++)
				  {
				    double tdist = Dist2(pt, GetPoint (innerchartpoints.Get(l)));
				    if (tdist < 4 * h2)
				      {
					accepted = 1; 
					break;
				      }
				  }
                                */
                                for (int l = 0; l < innerchartpts.Size(); l++)
				  {
				    double tdist = Dist2(pt, innerchartpts[l]);
				    if (tdist < 4 * h2)
				      {
					accepted = 1; 
					break;
				      }
				  }
				if (accepted) break;
			      }
                          
                          // NgProfiler::StopTimer (timer5csl);
			}
                      // NgProfiler::StartTimer (timer5b);
		      
		      if (accepted)
			{
			  changed = true;
			  outermark.Elem(nt) = chartnum;
			  
			  if (GetMarker(nt) != chartnum)
			    {
			      chartbound.AddTriangle(GetTriangle(nt));
			      chart->AddOuterTrig(nt);
			      for (int k = 1; k <= 3; k++)
				{
				  if (pointstochart.Get(GetTriangle(nt).PNum(k))
				      != chartnum) 
				    {
				      pointstochart.Elem(GetTriangle(nt).PNum(k)) = chartnum;
				      chartpoints.Append(GetTriangle(nt).PNum(k));
				    }
				}
			    }
			}
                      // NgProfiler::StopTimer (timer5b);
		    }	       
		}
	    }
	}            

      // NgProfiler::StopTimer (timer3);
      // NgProfiler::StartTimer (timere);      
      // NgProfiler::StartTimer (timere1);      
      //end of while loop for outer chart
      GetDirtyChartTrigs(chartnum, *chart, outermark, chartpointchecked, dirtycharttrigs);
      //dirtycharttrigs are local (chart) point numbers!!!!!!!!!!!!!!!!

      if (dirtycharttrigs.Size() != 0 && 
	  (dirtycharttrigs.Size() != chart->GetNChartT() || dirtycharttrigs.Size() != 1))
	{
	  if (dirtycharttrigs.Size() == chart->GetNChartT() && dirtycharttrigs.Size() != 1)
	    {
	      //if all trigs would be eliminated -> leave 1 trig!
	      dirtycharttrigs.SetSize(dirtycharttrigs.Size() - 1);
	    }
	  for (int k = 1; k <= dirtycharttrigs.Size(); k++)
	    {
	      int tn = chart->GetChartTrig(dirtycharttrigs.Get(k));
	      outermark.Elem(tn) = 0; //not necessary, for later use
	      SetMarker(tn, 0); 
	      markedtrigcnt--;
	      workedarea -= GetTriangle(tn).Area(points);
	    }
	  chart->MoveToOuterChart(dirtycharttrigs);
	  lastunmarked = 1;
	  lastunmarked = prelastunmarked;
	}

      chartbound.DeleteSearchTree();

      // NgProfiler::StopTimer (timere1);      
      // NgProfiler::StartTimer (timere2);      

      
      // cout << "key" << endl;
      // char key;
      // cin >> key;
      //calculate an estimate meshsize, not to produce too large outercharts, with factor 2 larger!
      RestrictHChartDistOneChart(chartnum, chartdistacttrigs, mesh, h, 0.5, atlasminh);
      // NgProfiler::Print(stdout);
      // NgProfiler::StopTimer (timere2);      
      
      // NgProfiler::StopTimer (timere);      
      
    }
  
  // NgProfiler::StopTimer (timer1);
  // NgProfiler::Print(stdout);


  PrintMessage(5,"");
  PrintMessage(5,"NO charts=", atlas.Size());

  int cnttrias = 0;
  outerchartspertrig.SetSize(GetNT());
  for (int i = 1; i <= atlas.Size(); i++)
    {
      for (int j = 1; j <= GetChart(i).GetNT(); j++)
	{
	  int tn = GetChart(i).GetTrig(j);
	  AddOCPT(tn,i);
	}
      
      cnttrias += GetChart(i).GetNT();
    }
  PrintMessage(5, "NO outer chart trias=", cnttrias);

  //sort outerchartspertrig
  for (int i = 1; i <= GetNT(); i++)
    {
      for (int k = 1; k < GetNOCPT(i); k++)
	for (int j = 1; j < GetNOCPT(i); j++)
	  {
	    int swap = GetOCPT(i,j);
	    if (GetOCPT(i,j+1) < swap)
	      {
		SetOCPT(i,j,GetOCPT(i,j+1));
		SetOCPT(i,j+1,swap);
	      }
	  }
      
      // check make atlas
      if (GetChartNr(i) <= 0 || GetChartNr(i) > GetNOCharts()) 
	PrintSysError("Make Atlas: chartnr(", i, ")=0!!");
    }

  mesh.SetGlobalH(mparam.maxh);
  mesh.SetMinimalH(mparam.minh);
  
  
  AddConeAndSpiralEdges();
  
  PrintMessage(5,"Make Atlas finished");

  PopStatus();
}


int STLGeometry::TrigIsInOC(int tn, int ocn) const
{
  if (tn < 1 || tn > GetNT())
    {
      // assert (1);
      abort ();
      PrintSysError("STLGeometry::TrigIsInOC illegal tn: ", tn);
      
      return 0;
    }

  /*
  int firstval = 0;
  int i;
  for (i = 1; i <= GetNOCPT(tn); i++)
    {
      if (GetOCPT(tn, i) == ocn) {firstval = 1;}
    }
  */

  int found = 0;

  int inc = 1;
  while (inc <= GetNOCPT(tn)) {inc *= 2;}
  inc /= 2;

  int start = inc;

  while (!found && inc > 0)
    {
      if (GetOCPT(tn,start) > ocn) {inc = inc/2; start -= inc;}
      else if (GetOCPT(tn,start) < ocn) {inc = inc/2; if (start+inc <= GetNOCPT(tn)) {start += inc;}}
      else {found = 1;}
    }

  return GetOCPT(tn, start) == ocn;
}

int STLGeometry :: GetChartNr(int i) const
{
  if (i > chartmark.Size()) 
    {
      PrintSysError("GetChartNr(", i, ") not possible!!!");
      i = 1;
    }
  return chartmark.Get(i);
}
/*
int STLGeometry :: GetMarker(int i) const
{
  return chartmark.Get(i);
}
*/
void STLGeometry :: SetMarker(int nr, int m) 
{
  chartmark.Elem(nr) = m;
}
int STLGeometry :: GetNOCharts() const
{
  return atlas.Size();
}
const STLChart& STLGeometry :: GetChart(int nr) const 
{
  if (nr > atlas.Size()) 
    {
      PrintSysError("GetChart(", nr, ") not possible!!!");
      nr = 1;
    }
  return *(atlas.Get(nr));
}

int STLGeometry :: AtlasMade() const
{
  return chartmark.Size() != 0;
}


/*
//return 1 if not exists
int AddIfNotExists(Array<int>& list, int x)
{
  int i;
  for (i = 1; i <= list.Size(); i++)
    {
      if (list.Get(i) == x) {return 0;} 
    }
  list.Append(x);
  return 1;
}
*/

void STLGeometry :: GetInnerChartLimes(Array<twoint>& limes, int chartnum)
{
  int j, k;
  
  int t, nt, np1, np2;
  
  limes.SetSize(0);

  STLChart& chart = GetChart(chartnum);

  for (j = 1; j <= chart.GetNChartT(); j++)
    {
      t = chart.GetChartTrig(j); 
      const STLTriangle& tt = GetTriangle(t);
      for (k = 1; k <= 3; k++)
	{
	  nt = NeighbourTrig(t,k); 
	  if (GetChartNr(nt) != chartnum)
	    {	      
	      tt.GetNeighbourPoints(GetTriangle(nt),np1,np2);
	      if (!IsEdge(np1,np2))
		{
		  limes.Append(twoint(np1,np2));
		  /*
		  p3p1 = GetPoint(np1);
		  p3p2 = GetPoint(np2);
		  if (AddIfNotExists(limes,np1)) 
		    {
		      plimes1.Append(p3p1); 
		      //plimes1trigs.Append(t);
		      //plimes1origin.Append(np1);
		    }
		  if (AddIfNotExists(limes1,np2)) 
		    {
		      plimes1.Append(p3p2); 
		      //plimes1trigs.Append(t);
		      //plimes1origin.Append(np2); 			      
		    }
		  //chart.AddILimit(twoint(np1,np2));
		  
		  for (int di = 1; di <= divisions; di++)
		    {
		      double f1 = (double)di/(double)(divisions+1.);
		      double f2 = (divisions+1.-(double)di)/(double)(divisions+1.);
		      
		      plimes1.Append(Point3d(p3p1.X()*f1+p3p2.X()*f2,
					     p3p1.Y()*f1+p3p2.Y()*f2,
					     p3p1.Z()*f1+p3p2.Z()*f2));
		      //plimes1trigs.Append(t);
		      //plimes1origin.Append(0); 			      
		    }
		  */
		}
	    }
	}
    }
}
	 


void STLGeometry :: GetDirtyChartTrigs(int chartnum, STLChart& chart,
				       const Array<int>& outercharttrigs,
				       Array<int>& chartpointchecked,
				       Array<int>& dirtytrigs)
{
  dirtytrigs.SetSize(0);
  int j,k,n;

  int np1, np2, nt;
  int cnt = 0;

  for (j = 1; j <= chart.GetNChartT(); j++)
    {
      int t = chart.GetChartTrig(j); 
      const STLTriangle& tt = GetTriangle(t);
      
      for (k = 1; k <= 3; k++)
	{
	  nt = NeighbourTrig(t,k); 
	  if (GetChartNr(nt) != chartnum && outercharttrigs.Get(nt) != chartnum)
	    {	      
	      tt.GetNeighbourPoints(GetTriangle(nt),np1,np2);
	      if (!IsEdge(np1,np2))
		{
		  dirtytrigs.Append(j); //local numbers!!!
		  cnt++;
		  break; //only once per trig!!!
		}
	    }
	}
    }
  cnt = 0;

  int ap1, ap2, tn1, tn2, l, problem, pn;
  Array<int> trigsaroundp;

  for (j = chart.GetNChartT(); j >= 1; j--)
    {
      int t = chart.GetChartTrig(j); 
      const STLTriangle& tt = GetTriangle(t);
      
      for (k = 1; k <= 3; k++)
	{
	  pn = tt.PNum(k);
	  //if (chartpointchecked.Get(pn) == chartnum)
	  //{continue;}
	  
	  int checkpoint = 0;
	  for (n = 1; n <= trigsperpoint.EntrySize(pn); n++)
	    {
	      if (trigsperpoint.Get(pn,n) != t && //ueberfluessig???
		  GetChartNr(trigsperpoint.Get(pn,n)) != chartnum &&
		  outercharttrigs.Get(trigsperpoint.Get(pn,n)) != chartnum) {checkpoint = 1;};
	    }
	  if (checkpoint)
	    {
	      chartpointchecked.Elem(pn) = chartnum;

	      GetSortedTrianglesAroundPoint(pn,t,trigsaroundp);
	      trigsaroundp.Append(t); //ring
	      
	      problem = 0;
	      //forward:
	      for (l = 2; l <= trigsaroundp.Size()-1; l++)
		{
		  tn1 = trigsaroundp.Get(l-1);
		  tn2 = trigsaroundp.Get(l);
		  const STLTriangle& t1 = GetTriangle(tn1);
		  const STLTriangle& t2 = GetTriangle(tn2);
		  t1.GetNeighbourPoints(t2, ap1, ap2);
		  if (IsEdge(ap1,ap2)) break;
		  
		  if (GetChartNr(tn2) != chartnum && outercharttrigs.Get(tn2) != chartnum) {problem = 1;}
		}

	      //backwards:
	      for (l = trigsaroundp.Size()-1; l >= 2; l--)
		{
		  tn1 = trigsaroundp.Get(l+1);
		  tn2 = trigsaroundp.Get(l);
		  const STLTriangle& t1 = GetTriangle(tn1);
		  const STLTriangle& t2 = GetTriangle(tn2);
		  t1.GetNeighbourPoints(t2, ap1, ap2);
		  if (IsEdge(ap1,ap2)) break;
		  
		  if (GetChartNr(tn2) != chartnum && outercharttrigs.Get(tn2) != chartnum) {problem = 1;}
		}
	      if (problem && !IsInArray(j,dirtytrigs))
		{
		  dirtytrigs.Append(j);
		  cnt++;
		  break; //only once per triangle
		}
	    }
	}
    }
  
}

}
