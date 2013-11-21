#include <fstream>
#include <iomanip>

#include "TideIndexApplication.h"
#include "TideMatchSet.h"
#include "TideSearchApplication.h"

extern AA_MOD_T* list_of_mods[MAX_AA_MODS]; // list containing all aa mods
extern int num_mods;  // ANY_POSITION mods

string TideMatchSet::cleavage_type_ = "";
char TideMatchSet::match_collection_loc_[] = {0};
char TideMatchSet::decoy_match_collection_loc_[] = {0};

TideMatchSet::TideMatchSet(
  Arr* matches,
  double max_mz
) :
  matches_(matches), max_mz_(max_mz) {
  if (cleavage_type_.empty()) {
    ENZYME_T enzyme = get_enzyme_type_parameter("enzyme");
    char* enzyme_string = enzyme_type_to_string(enzyme);
    DIGEST_T digestion = get_digest_type_parameter("digestion");
    char* digestion_string = digest_type_to_string(digestion);
    cleavage_type_ = enzyme_string;
    cleavage_type_ += '-';
    cleavage_type_ += digestion_string;
    free(enzyme_string);
    free(digestion_string);
  }
  exact_pval_search = false;
}
/*
TideMatchSet::TideMatchSet(
  Arr2* matches,
  double max_mz
) :
  matches2_(matches), max_mz_(max_mz) {
  if (cleavage_type_.empty()) {
    ENZYME_T enzyme = get_enzyme_type_parameter("enzyme");
    char* enzyme_string = enzyme_type_to_string(enzyme);
    DIGEST_T digestion = get_digest_type_parameter("digestion");
    char* digestion_string = digest_type_to_string(digestion);
    cleavage_type_ = enzyme_string;
    cleavage_type_ += '-';
    cleavage_type_ += digestion_string;
    free(enzyme_string);
    free(digestion_string);
  }
}
*/
TideMatchSet::~TideMatchSet() {
}

/**
 * Write matches to output files
 * This is for writing tab-delimited only
 */
void TideMatchSet::report(
  ofstream* target_file,  ///< target file to write to
  ofstream* decoy_file, ///< decoy file to write to
  int top_n,  ///< number of matches to report
  const Spectrum* spectrum, ///< spectrum for matches
  int charge, ///< charge for matches
  const ActivePeptideQueue* peptides, ///< peptide queue
  const ProteinVec& proteins,  ///< proteins corresponding with peptides
  bool compute_sp ///< whether to compute sp or not
) {
  if (matches_->size() == 0) {
    return;
  }

  carp(CARP_DETAILED_DEBUG, "Tide MatchSet reporting top %d of %d matches",
       top_n, matches_->size());

  vector<Arr::iterator> targets, decoys;
  gatherTargetsAndDecoys(peptides, proteins, targets, decoys, top_n);

  map<Arr::iterator, FLOAT_T> delta_cn_map;
  computeDeltaCns(targets, &delta_cn_map);
  computeDeltaCns(decoys, &delta_cn_map);

  map<Arr::iterator, pair<const SpScorer::SpScoreData, int> > sp_map;
  if (compute_sp) {
    SpScorer sp_scorer(proteins, *spectrum, charge, max_mz_);
    computeSpData(targets, &sp_map, &sp_scorer, peptides);
    computeSpData(decoys, &sp_map, &sp_scorer, peptides);
  }

  writeToFile(target_file, targets, false, spectrum, charge,
              peptides, proteins, delta_cn_map, compute_sp ? &sp_map : NULL);
  writeToFile(decoy_file, decoys, true, spectrum, charge,
              peptides, proteins, delta_cn_map, compute_sp ? &sp_map : NULL);
}

/**
 * Helper function for tab delimited report function
 */
void TideMatchSet::writeToFile(
  ofstream* file,
  const vector<Arr::iterator>& vec,
  bool decoyVec,
  const Spectrum* spectrum,
  int charge,
  const ActivePeptideQueue* peptides,
  const ProteinVec& proteins,
  const map<Arr::iterator, FLOAT_T>& delta_cn_map,
  const map<Arr::iterator, pair<const SpScorer::SpScoreData, int> >* sp_map
) {
  if (!file) {
    return;
  }

  int cur = 0;
  for (vector<Arr::iterator>::const_iterator i = vec.begin(); i != vec.end(); ++i) {
    const Peptide& peptide = *(peptides->GetPeptide((*i)->second));
    const pb::Protein& protein = *(proteins[peptide.FirstLocProteinId()]);

    string proteinName = getProteinName(protein, peptide, NULL);

    map<size_t, double> modMap; // AA index -> mod delta
    const ModCoder::Mod* mods;
    int pep_mods = peptide.Mods(&mods);
    for (int j = 0; j < pep_mods; ++j) {
      int mod_index; // 0 based
      double mod_delta;
      MassConstants::DecodeMod(mods[j], &mod_index, &mod_delta);
      map<size_t, double>::iterator lookup = modMap.find(mod_index);
      if (lookup == modMap.end()) {
        modMap[mod_index] = mod_delta;
      } else {
        modMap[mod_index] += mod_delta;
      }
    }
    string seq = peptide.Seq();
    for (size_t j = seq.length() - 1; j >= 0 && !modMap.empty(); --j) {
      map<size_t, double>::iterator lookup = modMap.find(j);
      if (lookup == modMap.end()) {
        continue;
      }
      stringstream ss;
      ss << '[' << lookup->second << ']';
      modMap.erase(lookup);
      seq.insert(j + 1, ss.str());
    }

    string n_term, c_term;
    getFlankingAAs(&peptide, &protein, &n_term, &c_term);
    const SpScorer::SpScoreData* sp_data = sp_map ? &(sp_map->at(*i).first) : NULL;

    *file << spectrum->SpectrumNumber() << '\t'
          << charge << '\t'
          << spectrum->PrecursorMZ() << '\t'
          << (spectrum->PrecursorMZ() - MASS_PROTON) * charge << '\t'
          << peptide.Mass() << '\t'
          << delta_cn_map.at(*i) << '\t';
    if (sp_map) {
      *file << sp_data->sp_score << '\t'
            << sp_map->at(*i).second << '\t';
    }
    *file << ((*i)->first.first) << '\t';
      if (exact_pval_search)
	    *file << ((*i)->first.second) << '\t';
     *file << ++cur << '\t';
    if (sp_map) {
      *file << sp_data->matched_ions << '\t'
            << sp_data->total_ions << '\t';
    }
    const string& residues = protein.residues();
    *file << matches_->size() << '\t'
          << seq << '\t'
          << cleavage_type_ << '\t'
          << proteinName << '\t'
          << n_term << c_term;
    if (decoyVec) {
      *file << '\t'
            << residues.substr(residues.length() - peptide.Len());
    }
    *file << endl;
  }
}

/**
 * Write matches to output files
 */
void TideMatchSet::report(
  OutputFiles* output_files,  ///< pointer to output handler
  int top_n,  ///< number of matches to report
  const Spectrum* spectrum, ///< spectrum for matches
  int charge, ///< charge for matches
  const ActivePeptideQueue* peptides, ///< peptide queue
  const ProteinVec& proteins,  ///< proteins corresponding with peptides
  bool compute_sp ///< whether to compute sp or not
) {
  if (matches_->size() == 0) {
    return;
  }

  carp(CARP_DETAILED_DEBUG, "Tide MatchSet reporting top %d of %d matches",
       top_n, matches_->size());

  vector<Arr::iterator> targets, decoys;
  gatherTargetsAndDecoys(peptides, proteins, targets, decoys, top_n);

  MatchCollection* crux_collection =
    new(match_collection_loc_) MatchCollection();
  MatchCollection* crux_decoy_collection =
    new(decoy_match_collection_loc_) MatchCollection();
  vector<PostProcessProtein*> proteins_made;

  // For Sp scoring
  FLOAT_T lowest_sp = BILLION;
  SpScorer* sp_scorer = (compute_sp) ?
    new SpScorer(proteins, *spectrum, charge, max_mz_) : NULL;

  // Create a Crux spectrum and z state
  Crux::Spectrum crux_spectrum(
    spectrum->SpectrumNumber(), spectrum->SpectrumNumber(),
    spectrum->PrecursorMZ(), vector<int>(1, charge), "");
  SpectrumZState z_state;
  z_state.setMZ(crux_spectrum.getPrecursorMz(), charge);

  crux_collection->exact_pval_search = exact_pval_search;
  crux_decoy_collection->exact_pval_search = exact_pval_search;

  addCruxMatches(crux_collection, &proteins_made, targets, false,
                 crux_spectrum, peptides, proteins, z_state, sp_scorer, &lowest_sp);
  addCruxMatches(crux_decoy_collection, &proteins_made, decoys, true,
                 crux_spectrum, peptides, proteins, z_state, sp_scorer, &lowest_sp);

  if (sp_scorer) {
    crux_spectrum.setTotalEnergy(sp_scorer->TotalIonIntensity());
    crux_spectrum.setLowestSp(lowest_sp);
    delete sp_scorer;
  }

  // Write matches
  vector<MatchCollection*> decoy_vector(1, crux_decoy_collection);
  output_files->writeMatches(crux_collection, decoy_vector, XCORR, &crux_spectrum);

  // Clean up
  crux_collection->~MatchCollection();
  crux_decoy_collection->~MatchCollection();
  for (vector<PostProcessProtein*>::iterator i = proteins_made.begin();
       i != proteins_made.end();
       ++i) {
    delete *i;
  }
}

/**
 * Helper function for normal report function
 */
void TideMatchSet::addCruxMatches(
  MatchCollection* match_collection,
  vector<PostProcessProtein*>* proteins_made,
  const vector<Arr::iterator>& vec,
  bool decoyVec,
  Crux::Spectrum& crux_spectrum,
  const ActivePeptideQueue* peptides,
  const ProteinVec& proteins,
  SpectrumZState& z_state,
  SpScorer* sp_scorer,
  FLOAT_T* lowest_sp_out
) {

  FLOAT_T lnNumSp = log(matches_->size());
  
  // Create a Crux match for each match
  for (vector<Arr::iterator>::const_iterator i = vec.begin(); i != vec.end(); ++i) {
    const Peptide* peptide = peptides->GetPeptide((*i)->second);
    const pb::Protein* protein = proteins[peptide->FirstLocProteinId()];

    PostProcessProtein* new_protein;
    Crux::Match* match = getCruxMatch(peptide, protein, &crux_spectrum, z_state,
                                      &new_protein);
    proteins_made->push_back(new_protein);
    match_collection->addMatch(match);
    if (decoyVec) {
      match->setNullPeptide(true);
    }
    Crux::Match::freeMatch(match); // so match gets deleted when collection does

    // Set Xcorr score in match
    match->setScore(XCORR, (*i)->first.first);
    match->setScore(TIDE_SEARCH_EXACT_PVAL, (*i)->first.first);
    match->setScore(TIDE_SEARCH_REFACTORED_XCORR, (*i)->first.second);

    // Set lnNumSp in match
    match->setLnExperimentSize(lnNumSp);

    if (sp_scorer) {
      pb::Peptide* pb_peptide = getPbPeptide(*peptide);

      // Score for Sp
      SpScorer::SpScoreData sp_score_data;
      sp_scorer->Score(*pb_peptide, sp_score_data);
      delete pb_peptide;

      FLOAT_T sp = sp_score_data.sp_score;
      if (sp < *lowest_sp_out) {
        *lowest_sp_out = sp;
      }

      // Set Sp, B/Y scores in match
      match->setScore(SP, sp);
      match->setBYIonMatched(sp_score_data.matched_ions);
      match->setBYIonPossible(sp_score_data.total_ions);
    }
  }
  match_collection->setZState(z_state);
  match_collection->setExperimentSize(matches_->size());
  match_collection->populateMatchRank(XCORR);
  match_collection->forceScoredBy(XCORR);
  if (sp_scorer) {
    match_collection->populateMatchRank(SP);
    match_collection->forceScoredBy(SP);
  }
}

/**
 * Write headers for tab delimited file
 */
void TideMatchSet::writeHeaders(
  ofstream* file,
  bool decoyFile,
  bool sp,
  bool exact_pval_search
) {
  if (!file) {
    return;
  }
  const int headers[] = {
    SCAN_COL, CHARGE_COL, SPECTRUM_PRECURSOR_MZ_COL, SPECTRUM_NEUTRAL_MASS_COL,
    PEPTIDE_MASS_COL, DELTA_CN_COL, SP_SCORE_COL, SP_RANK_COL, XCORR_SCORE_COL,
    XCORR_RANK_COL, BY_IONS_MATCHED_COL, BY_IONS_TOTAL_COL,
    MATCHES_SPECTRUM_COL, SEQUENCE_COL, CLEAVAGE_TYPE_COL, PROTEIN_ID_COL,
    FLANKING_AA_COL, UNSHUFFLED_SEQUENCE_COL
  };
  size_t numHeaders = sizeof(headers) / sizeof(int);
  for (size_t i = 0; i < numHeaders; ++i) {
    int header = headers[i];
    if (!sp &&
        (header == SP_SCORE_COL || header == SP_RANK_COL ||
         header == BY_IONS_MATCHED_COL || header == BY_IONS_TOTAL_COL)) {
      continue;
    } else if (!decoyFile && header == UNSHUFFLED_SEQUENCE_COL) {
      continue;
    }
    if (i > 0) {
      *file << '\t';
    }
    if (exact_pval_search && header == XCORR_SCORE_COL)
     *file << get_column_header(EXACT_PVALUE_COL) << '\t' << get_column_header(REFACTORED_SCORE_COL);
    else 
     *file << get_column_header(header);
  }
  *file << endl;
}

/**
 * Create a Crux match from Tide data structures
 */
Crux::Match* TideMatchSet::getCruxMatch(
  const Peptide* peptide, ///< Tide peptide for match
  const pb::Protein* protein, ///< Tide protein for match
  Crux::Spectrum* crux_spectrum,  ///< Crux spectrum for match
  SpectrumZState& crux_z_state, ///< Crux z state for match
  PostProcessProtein** protein_made ///< out parameter for new protein
) {
  // Get flanking AAs
  string n_term, c_term;
  getFlankingAAs(peptide, protein, &n_term, &c_term);

  // Create protein
  PostProcessProtein* parent_protein = new PostProcessProtein();
  *protein_made = parent_protein;

  bool is_decoy;
  string proteinName = getProteinName(*protein, *peptide, &is_decoy);

  parent_protein->setId(proteinName.c_str());
  string unshuffledSeq = (!is_decoy) ? peptide->Seq() :
    protein->residues().substr(protein->residues().length() - peptide->Len());
  int start_idx = parent_protein->findStart(unshuffledSeq, n_term, c_term);

  // Create peptide
  Crux::Peptide* crux_peptide = new Crux::Peptide(
    peptide->Len(), peptide->Mass(), parent_protein, start_idx);

  // Set up modifications for peptide
  const ModCoder::Mod* mods;
  int pep_mods = peptide->Mods(&mods);
  MODIFIED_AA_T* mod_seq;
  convert_to_mod_aa_seq(peptide->Seq().c_str(), &mod_seq);
  for (int i = 0; i < pep_mods; ++i) {
    int mod_index; // 0 based
    double mod_delta;
    MassConstants::DecodeMod(mods[i], &mod_index, &mod_delta);
    // Look up mod and apply it to AA
    const AA_MOD_T* mod = lookUpMod(mod_delta);
    modify_aa(mod_seq + mod_index, mod);
  }
  crux_peptide->setModifiedAASequence(mod_seq, is_decoy);
  free(mod_seq);

  // Create match and return
  Crux::Match* match = new Crux::Match(
    crux_peptide, crux_spectrum, crux_z_state, false);

  return match;
}

/**
 * Returns a pointer to the modification in the list of mods, adding it if it
 * doesn't exist
 */
const AA_MOD_T* TideMatchSet::lookUpMod(double delta_mass) {
  for (int i = 0; i < num_mods; ++i) {
    const AA_MOD_T* mod = list_of_mods[i];
    if (aa_mod_get_mass_change(mod) == delta_mass) {
      carp(CARP_DETAILED_DEBUG, "Found existing mod (%f)", delta_mass);
      return mod;
    }
  }

  carp(CARP_DEBUG, "Adding new mod (%f)", delta_mass);
  AA_MOD_T* new_mod = new_aa_mod(num_mods);
  aa_mod_set_mass_change(new_mod, delta_mass);
  list_of_mods[num_mods] = new_mod;

  ++num_mods;

  return new_mod;
}

void TideMatchSet::gatherTargetsAndDecoys(
  const ActivePeptideQueue* peptides,
  const ProteinVec& proteins,
  vector<Arr::iterator>& targetsOut,
  vector<Arr::iterator>& decoysOut,
  int top_n
) {
  make_heap(matches_->begin(), matches_->end(), less_score());
  targetsOut.reserve(top_n);
  if (TideSearchApplication::hasDecoys()) {
    decoysOut.reserve(top_n);
    int popped = 0;
    do {
      pop_heap(matches_->begin(), matches_->end() - (popped++), less_score());
      Arr::iterator i = matches_->end() - popped;
      const Peptide& peptide = *(peptides->GetPeptide(i->second));
      const pb::Protein& protein = *(proteins[peptide.FirstLocProteinId()]);
      vector<Arr::iterator>* vec_ptr = !isDecoy(protein.name()) ? &targetsOut : &decoysOut;
      if (vec_ptr->size() < top_n) {
        vec_ptr->push_back(i);
      }
    } while ((targetsOut.size() != top_n || decoysOut.size() != top_n) &&
             popped < matches_->size());
  } else {
    for (int i = 0; i < min(top_n, matches_->size()); ++i) {
      pop_heap(matches_->begin(), matches_->end() - i, less_score());
      targetsOut.push_back(matches_->end() - i - 1);
    }
  }
}

/**
 * Create a pb peptide from Tide peptide
 */
pb::Peptide* TideMatchSet::getPbPeptide(
  const Peptide& peptide
) {

  pb::Peptide* pb_peptide = new pb::Peptide();
  pb_peptide->set_id(peptide.Id());
  pb_peptide->set_mass(peptide.Mass());
  pb_peptide->set_length(peptide.Len());
  if (peptide.HasAuxLocationsIndex()) {
    pb_peptide->set_aux_locations_index(peptide.AuxLocationsIndex());
  }

  // Copy over all the modifications for this Peptide
  const ModCoder::Mod* mods;
  int pep_mods = peptide.Mods(&mods);
  for (int i = 0; i < pep_mods; ++i) {
    pb_peptide->add_modifications(mods[i]);
  }

  // Copy over the Peptide's first location within the first protein
  pb::Location* first_location = pb_peptide->mutable_first_location();
  first_location->set_protein_id(peptide.FirstLocProteinId());
  first_location->set_pos(peptide.FirstLocPos());

  return pb_peptide;
}

/**
 * Gets the protein name with the index appended.
 * Optionally, can pass in a boolean pointer to be set to whether decoy or not
 */
string TideMatchSet::getProteinName(
  const pb::Protein& protein,
  const Peptide& peptide,
  bool* is_decoy
) {
  bool decoy = isDecoy(protein.name());
  if (is_decoy != NULL) {
    *is_decoy = decoy;
  }

  string proteinName;
  if (decoy) {
    // DecoyMagicByte + index + '.' + protein name
    proteinName = protein.name();
    size_t dot = proteinName.find('.');
    proteinName += '(' + proteinName.substr(1, dot - 1) + ')';
    proteinName.erase(0, dot + 1);
  } else {
    stringstream proteinNameStream;
    proteinNameStream << protein.name()
                      << '(' << peptide.FirstLocPos() + 1 << ')';
    proteinName = proteinNameStream.str();
  }
  return proteinName;
}

/**
 * Determine if the protein is a decoy protein.
 */
bool TideMatchSet::isDecoy(
  const string& proteinName
) {
  return !proteinName.empty() &&
         proteinName[0] == TideIndexApplication::DecoyMagicByte;
}

/**
 * Gets the flanking AAs for a Tide peptide sequence
 */
void TideMatchSet::getFlankingAAs(
  const Peptide* peptide, ///< Tide peptide to get flanking AAs for
  const pb::Protein* protein, ///< Tide protein for the peptide
  string* out_n,  ///< out parameter for n flank
  string* out_c ///< out parameter for c flank
) {
  int idx_n = peptide->FirstLocPos() - 1;
  int idx_c = peptide->FirstLocPos() + peptide->Len();
  const string& seq = protein->residues();

  *out_n = (idx_n >= 0) ?
    seq.substr(idx_n, 1) : "-";
  *out_c = (idx_c < seq.length()) ?
    seq.substr(idx_c, 1) : "-";
}

void TideMatchSet::computeDeltaCns(
  const vector<Arr::iterator>& vec, // xcorr*100000000.0, high to low
  map<Arr::iterator, FLOAT_T>* delta_cn_map // map to add delta cn scores to
) {
  FLOAT_T lastXcorr = BILLION;
  for (vector<Arr::iterator>::const_reverse_iterator i = vec.rbegin();
       i != vec.rend();
       ++i) {
    const FLOAT_T xcorr = (*i)->first.first;
    delta_cn_map->insert(make_pair(*i, (lastXcorr == BILLION) ?
      0 : (xcorr - lastXcorr) / max(xcorr, FLOAT_T(1))));
    lastXcorr = xcorr;
  }
}

void TideMatchSet::computeSpData(
  const vector<Arr::iterator>& vec,
  map<Arr::iterator, pair<const SpScorer::SpScoreData, int> >* sp_rank_map,
  SpScorer* sp_scorer,
  const ActivePeptideQueue* peptides
) {
  vector< pair<Arr::iterator, SpScorer::SpScoreData> > spData;
  spData.reserve(vec.size());
  for (vector<Arr::iterator>::const_iterator i = vec.begin(); i != vec.end(); ++i) {
    spData.push_back(make_pair(*i, SpScorer::SpScoreData()));
    const Peptide& peptide = *(peptides->GetPeptide((*i)->second));
    pb::Peptide* pb_peptide = getPbPeptide(peptide);
    sp_scorer->Score(*pb_peptide, spData.back().second);
    delete pb_peptide;
  }
  sort(spData.begin(), spData.end(), spGreater());
  for (size_t i = 0; i < spData.size(); ++i) {
    sp_rank_map->insert(make_pair(
      spData[i].first, make_pair(spData[i].second, i + 1)));
  }
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 2
 * End:
 */
