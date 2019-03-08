
#include <WarpX.H>

using namespace amrex;

/** \brief Takes an array of 3 MultiFab `vector_field`
  * (representing the x, y, z components of a vector),
  * averages it to the cell center, and stores the
  * resulting MultiFab in mf_avg (in the components dcomp to dcomp+2)
  */
void
AverageAndPackVectorField( Vector<std::unique_ptr<MultiFab> > mf_avg,
    Vector<std::array< std::unique_ptr<MultiFab>, 3 >> vector_field,
    const int dcomp, const int lev, const int ngrow )
{

    // Needed since `average_edge_to_cellcenter` requires fields to
    // be passed as Vector
    Vector<const MultiFab*> srcmf(AMREX_SPACEDIM);

    // Check the type of staggering of the 3-component `vector_field`
    // and average accordingly:
    // - Fully cell-centered field (no average needed; simply copy)
    if ( std::all_of( 0, AMREX_SPACEDIM,
        [](int i){ return vector_field[lev][i]->is_cell_centered(); } ) ){

        MultiFab::Copy(*mf_avg[lev], *vector_field[lev][0], 0, dcomp  , 1, ngrow);
        MultiFab::Copy(*mf_avg[lev], *vector_field[lev][1], 0, dcomp+1, 1, ngrow);
        MultiFab::Copy(*mf_avg[lev], *vector_field[lev][2], 0, dcomp+2, 1, ngrow);

    // - Fully nodal
    } else if ( std::all_of( 0, AMREX_SPACEDIM,
        [](int i){ return vector_field[lev][i]->is_nodal(); } ) ){

        amrex::average_node_to_cellcenter(*mf_avg[lev], dcomp  , *vector_field[lev][0], 0, 1);
        amrex::average_node_to_cellcenter(*mf_avg[lev], dcomp+1, *vector_field[lev][1], 0, 1);
        amrex::average_node_to_cellcenter(*mf_avg[lev], dcomp+2, *vector_field[lev][2], 0, 1);

    // - Staggered, in the same way as E on a Yee grid
    } else if ( std::all_of( 0, AMREX_SPACEDIM,
        [](int i){ return vector_field[lev][i]->is_cell_centered(i); } ) ){

        PackPlotDataPtrs(srcmf, vector);
        amrex::average_edge_to_cellcenter(*mf_avg[lev], dcomp, srcmf);
#if (AMREX_SPACEDIM == 2)
        MultiFab::Copy(*mf_avg[lev], *mf_avg[lev], dcomp+1, dcomp+2, 1, ngrow);
        MultiFab::average_node_to_cellcenter(*mf_avg[lev], *vector_field[lev][1], 0, dcomp+1, 1, ngrow);
#endif

    // - Staggered, in the same way as B on a Yee grid
    } else if ( std::all_of( 0, AMREX_SPACEDIM,
        [](int i){ return vector_field[lev][i]->is_nodal(i); } ) ){

        PackPlotDataPtrs(srcmf, vector);
        amrex::average_face_to_cellcenter(*mf_avg[lev], dcomp, srcmf);
#if (AMREX_SPACEDIM == 2)
        MultiFab::Copy(*mf_avg[lev], *mf_avg[lev], dcomp+1, dcomp+2, 1, ngrow);
        MultiFab::Copy(*mf_avg[lev], *vector_field[lev][1], 0, dcomp+1, 1, ngrow);
#endif

    } else {
        AMREX_ALWAYS_ASSERT_WITH_MESSAGE(false, "Unknown staggering.");
    }
}


/** \brief Write the different fields of the simulation,
* averaged to the cell center of each cell (default WarpX output)
*/
void
WarpX::WriteAveragedFields ( const std::string& plotfilename ) const
{

    Vector<std::string> varnames; // Name of the written fields
    // mf_avg will contain the averaged, cell-centered fields
    Vector<std::unique_ptr<MultiFab> > mf_avg(finest_level+1);

  // Count how many different fields should be written (ncomp)
  const int ncomp = 3*3
  + static_cast<int>(plot_part_per_cell)
  + static_cast<int>(plot_part_per_grid)
  + static_cast<int>(plot_part_per_proc)
  + static_cast<int>(plot_proc_number)
  + static_cast<int>(plot_divb)
  + static_cast<int>(plot_dive)
  + static_cast<int>(plot_rho)
  + static_cast<int>(plot_F)
  + static_cast<int>(plot_finepatch)*6
  + static_cast<int>(plot_crsepatch)*6
  + static_cast<int>(costs[0] != nullptr);

  // Loop over levels of refinement
  for (int lev = 0; lev <= finest_level; ++lev)
  {
    const int ngrow = 0;
    mf_avg[lev].reset(new MultiFab(grids[lev], dmap[lev], ncomp, ngrow));

    int dcomp = 0;

    // j
    if (do_nodal)
    {
      amrex::average_node_to_cellcenter(*mf_avg[lev], dcomp  , *current_fp[lev][0], 0, 1);
      amrex::average_node_to_cellcenter(*mf_avg[lev], dcomp+1, *current_fp[lev][1], 0, 1);
      amrex::average_node_to_cellcenter(*mf_avg[lev], dcomp+2, *current_fp[lev][2], 0, 1);
    }
    else
    {
      PackPlotDataPtrs(srcmf, current_fp[lev]);
      amrex::average_edge_to_cellcenter(*mf_avg[lev], dcomp, srcmf);
      #if (AMREX_SPACEDIM == 2)
      MultiFab::Copy(*mf_avg[lev], *mf_avg[lev], dcomp+1, dcomp+2, 1, ngrow);
      amrex::average_node_to_cellcenter(*mf_avg[lev], dcomp+1, *current_fp[lev][1], 0, 1);
      #endif
    }
    if (lev == 0)
    {
      varnames.push_back("jx");
      varnames.push_back("jy");
      varnames.push_back("jz");
    }
    dcomp += 3;

    // E
    if (do_nodal)
    {
      amrex::average_node_to_cellcenter(*mf_avg[lev], dcomp  , *Efield_aux[lev][0], 0, 1);
      amrex::average_node_to_cellcenter(*mf_avg[lev], dcomp+1, *Efield_aux[lev][1], 0, 1);
      amrex::average_node_to_cellcenter(*mf_avg[lev], dcomp+2, *Efield_aux[lev][2], 0, 1);
    }
    else
    {
      PackPlotDataPtrs(srcmf, Efield_aux[lev]);
      amrex::average_edge_to_cellcenter(*mf_avg[lev], dcomp, srcmf);
      #if (AMREX_SPACEDIM == 2)
      MultiFab::Copy(*mf_avg[lev], *mf_avg[lev], dcomp+1, dcomp+2, 1, ngrow);
      amrex::average_node_to_cellcenter(*mf_avg[lev], dcomp+1, *Efield_aux[lev][1], 0, 1);
      #endif
    }
    if (lev == 0)
    {
      varnames.push_back("Ex");
      varnames.push_back("Ey");
      varnames.push_back("Ez");
    }
    dcomp += 3;

    // B
    if (do_nodal)
    {
      amrex::average_node_to_cellcenter(*mf_avg[lev], dcomp  , *Bfield_aux[lev][0], 0, 1);
      amrex::average_node_to_cellcenter(*mf_avg[lev], dcomp+1, *Bfield_aux[lev][1], 0, 1);
      amrex::average_node_to_cellcenter(*mf_avg[lev], dcomp+2, *Bfield_aux[lev][2], 0, 1);
    }
    else
    {
      PackPlotDataPtrs(srcmf, Bfield_aux[lev]);
      amrex::average_face_to_cellcenter(*mf_avg[lev], dcomp, srcmf);
      #if (AMREX_SPACEDIM == 2)
      MultiFab::Copy(*mf_avg[lev], *mf_avg[lev], dcomp+1, dcomp+2, 1, ngrow);
      MultiFab::Copy(*mf_avg[lev], *Bfield_aux[lev][1], 0, dcomp+1, 1, ngrow);
      #endif
    }
    if (lev == 0)
    {
      varnames.push_back("Bx");
      varnames.push_back("By");
      varnames.push_back("Bz");
    }
    dcomp += 3;

    if (plot_part_per_cell)
    {
      MultiFab temp_dat(grids[lev],mf_avg[lev]->DistributionMap(),1,0);
      temp_dat.setVal(0);

      // MultiFab containing number of particles in each cell
      mypc->Increment(temp_dat, lev);
      MultiFab::Copy(*mf_avg[lev], temp_dat, 0, dcomp, 1, 0);
      if (lev == 0)
      {
        varnames.push_back("part_per_cell");
      }
      dcomp += 1;
    }

    if (plot_part_per_grid || plot_part_per_proc)
    {
      const Vector<long>& npart_in_grid = mypc->NumberOfParticlesInGrid(lev);

      if (plot_part_per_grid)
      {
        // MultiFab containing number of particles per grid (stored as constant for all cells in each grid)
        #ifdef _OPENMP
        #pragma omp parallel
        #endif
        for (MFIter mfi(*mf_avg[lev]); mfi.isValid(); ++mfi) {
          (*mf_avg[lev])[mfi].setVal(static_cast<Real>(npart_in_grid[mfi.index()]), dcomp);
        }
        if (lev == 0)
        {
          varnames.push_back("part_per_grid");
        }
        dcomp += 1;
      }

      if (plot_part_per_proc)
      {
        // MultiFab containing number of particles per process (stored as constant for all cells in each grid)
        long n_per_proc = 0;
        #ifdef _OPENMP
        #pragma omp parallel reduction(+:n_per_proc)
        #endif
        for (MFIter mfi(*mf_avg[lev]); mfi.isValid(); ++mfi) {
          n_per_proc += npart_in_grid[mfi.index()];
        }
        mf_avg[lev]->setVal(static_cast<Real>(n_per_proc), dcomp,1);
        if (lev == 0)
        {
          varnames.push_back("part_per_proc");
        }
        dcomp += 1;
      }
    }

    if (plot_proc_number)
    {
      // MultiFab containing the Processor ID
      #ifdef _OPENMP
      #pragma omp parallel
      #endif
      for (MFIter mfi(*mf_avg[lev]); mfi.isValid(); ++mfi) {
        (*mf_avg[lev])[mfi].setVal(static_cast<Real>(ParallelDescriptor::MyProc()), dcomp);
      }
      if (lev == 0)
      {
        varnames.push_back("proc_number");
      }
      dcomp += 1;
    }

    if (plot_divb)
    {
      if (do_nodal) amrex::Abort("TODO: do_nodal && plot_divb");
      ComputeDivB(*mf_avg[lev], dcomp, {Bfield_aux[lev][0].get(),Bfield_aux[lev][1].get(),Bfield_aux[lev][2].get()}, WarpX::CellSize(lev));
      if (lev == 0)
      {
        varnames.push_back("divB");
      }
      dcomp += 1;
    }

    if (plot_dive)
    {
      if (do_nodal) amrex::Abort("TODO: do_nodal && plot_dive");
      const BoxArray& ba = amrex::convert(boxArray(lev),IntVect::TheUnitVector());
      MultiFab dive(ba,DistributionMap(lev),1,0);
      ComputeDivE( dive, 0,
        {Efield_aux[lev][0].get(), Efield_aux[lev][1].get(), Efield_aux[lev][2].get()},
        WarpX::CellSize(lev)
      );
      amrex::average_node_to_cellcenter(*mf_avg[lev], dcomp, dive, 0, 1);
      if (lev == 0)
      {
        varnames.push_back("divE");
      }
      dcomp += 1;
    }

    if (plot_rho)
    {
      amrex::average_node_to_cellcenter(*mf_avg[lev], dcomp, *rho_fp[lev], 0, 1);
      if (lev == 0)
      {
        varnames.push_back("rho");
      }
      dcomp += 1;
    }

    if (plot_F)
    {
      amrex::average_node_to_cellcenter(*mf_avg[lev], dcomp, *F_fp[lev], 0, 1);
      if (lev == 0)
      {
        varnames.push_back("F");
      }
      dcomp += 1;
    }

    if (plot_finepatch)
    {
      if (do_nodal)
      {
        amrex::average_node_to_cellcenter(*mf_avg[lev], dcomp  , *Efield_fp[lev][0], 0, 1);
        amrex::average_node_to_cellcenter(*mf_avg[lev], dcomp+1, *Efield_fp[lev][1], 0, 1);
        amrex::average_node_to_cellcenter(*mf_avg[lev], dcomp+2, *Efield_fp[lev][2], 0, 1);
      }
      else
      {
        PackPlotDataPtrs(srcmf, Efield_fp[lev]);
        amrex::average_edge_to_cellcenter(*mf_avg[lev], dcomp, srcmf);
        #if (AMREX_SPACEDIM == 2)
        MultiFab::Copy(*mf_avg[lev], *mf_avg[lev], dcomp+1, dcomp+2, 1, ngrow);
        amrex::average_node_to_cellcenter(*mf_avg[lev], dcomp+1, *Efield_fp[lev][1], 0, 1);
        #endif
      }
      if (lev == 0)
      {
        varnames.push_back("Ex_fp");
        varnames.push_back("Ey_fp");
        varnames.push_back("Ez_fp");
      }
      dcomp += 3;

      if (do_nodal)
      {
        amrex::average_node_to_cellcenter(*mf_avg[lev], dcomp  , *Bfield_fp[lev][0], 0, 1);
        amrex::average_node_to_cellcenter(*mf_avg[lev], dcomp+1, *Bfield_fp[lev][1], 0, 1);
        amrex::average_node_to_cellcenter(*mf_avg[lev], dcomp+2, *Bfield_fp[lev][2], 0, 1);
      }
      else
      {
        PackPlotDataPtrs(srcmf, Bfield_fp[lev]);
        amrex::average_face_to_cellcenter(*mf_avg[lev], dcomp, srcmf);
        #if (AMREX_SPACEDIM == 2)
        MultiFab::Copy(*mf_avg[lev], *mf_avg[lev], dcomp+1, dcomp+2, 1, ngrow);
        MultiFab::Copy(*mf_avg[lev], *Bfield_fp[lev][1], 0, dcomp+1, 1, ngrow);
        #endif
      }
      if (lev == 0)
      {
        varnames.push_back("Bx_fp");
        varnames.push_back("By_fp");
        varnames.push_back("Bz_fp");
      }
      dcomp += 3;
    }

    if (plot_crsepatch)
    {
      // First the electric field
      if (lev == 0)
      {
        mf_avg[lev]->setVal(0.0, dcomp, 3, ngrow);
      }
      else
      {
        if (do_nodal) amrex::Abort("TODO: do_nodal && plot_crsepatch");
        std::array<std::unique_ptr<MultiFab>, 3> E = getInterpolatedE(lev);
        PackPlotDataPtrs(srcmf, E);
        amrex::average_edge_to_cellcenter(*mf_avg[lev], dcomp, srcmf);
        #if (AMREX_SPACEDIM == 2)
        MultiFab::Copy(*mf_avg[lev], *mf_avg[lev], dcomp+1, dcomp+2, 1, ngrow);
        amrex::average_node_to_cellcenter(*mf_avg[lev], dcomp+1, *E[1], 0, 1);
        #endif
      }
      if (lev == 0)
      {
        varnames.push_back("Ex_cp");
        varnames.push_back("Ey_cp");
        varnames.push_back("Ez_cp");
      }
      dcomp += 3;

      // now the magnetic field
      if (lev == 0)
      {
        mf_avg[lev]->setVal(0.0, dcomp, 3, ngrow);
      }
      else
      {
        if (do_nodal) amrex::Abort("TODO: do_nodal && plot_crsepatch");
        std::array<std::unique_ptr<MultiFab>, 3> B = getInterpolatedB(lev);
        PackPlotDataPtrs(srcmf, B);
        amrex::average_face_to_cellcenter(*mf_avg[lev], dcomp, srcmf);
        #if (AMREX_SPACEDIM == 2)
        MultiFab::Copy(*mf_avg[lev], *mf_avg[lev], dcomp+1, dcomp+2, 1, ngrow);
        MultiFab::Copy(*mf_avg[lev], *B[1], 0, dcomp+1, 1, ngrow);
        #endif
      }
      if (lev == 0)
      {
        varnames.push_back("Bx_cp");
        varnames.push_back("By_cp");
        varnames.push_back("Bz_cp");
      }
      dcomp += 3;
    }

    if (costs[0] != nullptr)
    {
      MultiFab::Copy(*mf_avg[lev], *costs[lev], 0, dcomp, 1, 0);
      if (lev == 0)
      {
        varnames.push_back("costs");
      }
      dcomp += 1;
    }

    BL_ASSERT(dcomp == ncomp);
  }

  // Write the fields contained in `mf_avg`, and corresponding to the
  // names `varnames`, into a plotfile.
  // Prepare extra directory (filled later), for the raw fields
  Vector<std::string> rfs;
  if (plot_raw_fields) rfs.emplace_back("raw_fields");
  amrex::WriteMultiLevelPlotfile(
    plotfilename, finest_level+1,
    amrex::GetVecOfConstPtrs(mf_avg),
    varnames, Geom(), t_new[0], istep, refRatio(),
    "HyperCLaw-V1.1",
    "Level_",
    "Cell",
    rfs
  );

};
