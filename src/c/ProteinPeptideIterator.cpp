/**
 * \file ProteinPeptideIterator.cpp
 * $Revision: 0.0 $
 */

#include "ProteinPeptideIterator.h"

using namespace std;

/*
 * Takes a cumulative distribution of peptide masses (the mass_array) and
 * the start index and end index and returns a peptide mass
 */
FLOAT_T ProteinPeptideIterator::calculateSubsequenceMass (
    double* mass_array,
    int start_idx,
    int cur_length
  ){

  FLOAT_T mass_h2o = MASS_H2O_AVERAGE;
  if(get_mass_type_parameter("isotopic-mass") == MONO){
    mass_h2o = MASS_H2O_MONO;
  }

  // carp(CARP_DETAILED_DEBUG, "mass start = %i", start_idx);
  int end_idx = start_idx + cur_length;
  // carp(CARP_DETAILED_DEBUG, "mass end = %i", end_idx);
  FLOAT_T peptide_mass = mass_array[end_idx] - mass_array[start_idx] + mass_h2o;

  return peptide_mass;
}

/**
 * \brief Decide if a residue is in an inclusion list or is not in an
 * exclusion list. 
 *
 * For use with the user-specified enzyme digestion.  Takes an amino
 * acid, a list of amino acids, and a flag for if it is an inclusion
 * list or an exclusion list.  A cleavage can happen before/after the
 * given residue if it is either in the inclusion list or is not in
 * the exculsion list.
 * \returns TRUE if the residue is in the inclusion list or not in the
 * exclusion list.
 */
bool ProteinPeptideIterator::isResidueLegal(char aa, 
                           char* aa_list, 
                           int list_size, 
                           bool for_inclusion){

  // The logic for returning for_inclusion:
  // For an inclusion list (TRUE), once we find the aa it passes (TRUE)
  // For an exclusion list (FALSE), once we find the aa, it fails (FALSE)
  int idx=0;
  for(idx=0; idx < list_size; idx++){
    if( aa == aa_list[idx] ){ return for_inclusion; }
  }
  // or if we got to the end of the list and didn't find a match
  // for inclusion, it fails (!TRUE)
  // for exclusion, it passes (!FALSE)
  return ! for_inclusion;
}

/**
 * Compares the first and second amino acids in the given sequence to
 * see if they conform to the cleavage rules of the given enzyme.  For
 * NO_ENZYME, always returns TRUE.
 *
 * \returns TRUE if this is a valid cleavage position for the given enzyme.
 */
bool ProteinPeptideIterator::validCleavagePosition(
   char* sequence,
   //   PEPTIDE_TYPE_T cleavage
   ENZYME_T enzyme
){

  switch(enzyme){

  case TRYPSIN:
    if ((sequence[0] == 'K' || sequence[0] == 'R') && (sequence[1] != 'P')){
      return TRUE;
    } else {
      return FALSE;
    }
    break;
    
  case CHYMOTRYPSIN:
    if ((sequence[0] == 'F' || sequence[0] == 'W' || sequence[0] == 'Y') 
        && (sequence[1] != 'P')){
      return TRUE;
    } else {
      return FALSE;
    }
    break;
    break;

  case ELASTASE:
    if ((sequence[0] == 'A' || sequence[0] == 'L' ||
         sequence[0] == 'I' || sequence[0] == 'V') 
        && (sequence[1] != 'P')){
      return TRUE;
    } else {
      return FALSE;
    }
    break;

  case CLOSTRIPAIN:
    if (sequence[0] == 'R'){
      return TRUE;
    } else {
      return FALSE;
    }
    break;

  case CYANOGEN_BROMIDE:
    if (sequence[0] == 'M'){
      return TRUE;
    } else {
      return FALSE;
    }
    break;

  case IODOSOBENZOATE:
    if (sequence[0] == 'W'){
      return TRUE;
    } else {
      return FALSE;
    }
    break;

  case PROLINE_ENDOPEPTIDASE:
    if (sequence[0] == 'P'){
      return TRUE;
    } else {
      return FALSE;
    }
    break;

  case STAPH_PROTEASE:
    if (sequence[0] == 'E'){
      return TRUE;
    } else {
      return FALSE;
    }
    break;

  case ASPN:
    if (sequence[1] == 'D'){
      return TRUE;
    } else {
      return FALSE;
    }
    break;

  case MODIFIED_CHYMOTRYPSIN:
    if ((sequence[0] == 'F' || sequence[0] == 'L' ||
         sequence[0] == 'W' || sequence[0] == 'Y') 
        && (sequence[1] != 'P')){
      return TRUE;
    } else {
      return FALSE;
    }
    break;

  case ELASTASE_TRYPSIN_CHYMOTRYPSIN:
    if ((sequence[0] == 'A' || sequence[0] == 'L' ||
         sequence[0] == 'I' || sequence[0] == 'V' ||
         sequence[0] == 'K' || sequence[0] == 'R' ||
         sequence[0] == 'W' || sequence[0] == 'F' ||
         sequence[0] == 'Y' ) 
        && (sequence[1] != 'P')){
      return TRUE;
    } else {
      return FALSE;
    }
    break;

  case CUSTOM_ENZYME:
    //carp(CARP_FATAL, "The custom enzyme is not yet implmented.");

    return ( isResidueLegal(sequence[0], 
                              pre_cleavage_list,
                              pre_list_size, 
                              pre_for_inclusion)
             && 
             isResidueLegal(sequence[1], 
                              post_cleavage_list,
                              post_list_size, 
                              post_for_inclusion) );
    break;

  case NO_ENZYME:
    return TRUE;
    break;

  case INVALID_ENZYME:
  case NUMBER_ENZYME_TYPES:
    carp(CARP_FATAL, "Cannot generate peptides with invalid enzyme.");
    break;

  }// end switch

  return FALSE;
}

/**
 * \brief Adds cleavages to the protein peptide iterator that obey iterator
 * constraint.
 *
 * Uses the allowed cleavages arrays, and whether skipped cleavages
 * are allowed. 
 * A small inconsistency: 
 *  Allowed cleavages start at 0, while the output cleavages start at 1.
 */
void ProteinPeptideIterator::addCleavages(
    int* nterm_allowed_cleavages, 
    int  nterm_num_cleavages, 
    int* cterm_allowed_cleavages, 
    int  cterm_num_cleavages, 
    bool skip_cleavage_locations){

  // to avoid checking a lot of C-term before our current N-term cleavage
  int previous_cterm_cleavage_start= 0; 

  PEPTIDE_CONSTRAINT_T* constraint = peptide_constraint_;
  int nterm_idx, cterm_idx;

  // iterate over possible nterm and cterm cleavage locations
  for (nterm_idx=0; nterm_idx < nterm_num_cleavages; nterm_idx++){
    
    int next_cterm_cleavage_start = previous_cterm_cleavage_start;
    bool no_new_cterm_cleavage_start = true;
    for (cterm_idx = previous_cterm_cleavage_start; 
         cterm_idx < cterm_num_cleavages; cterm_idx++){

      // if we have skipped a cleavage location, break to next nterm
      if(
         (skip_cleavage_locations == false)
         &&
         ((*cumulative_cleavages_)[nterm_allowed_cleavages[nterm_idx]] 
          < 
          (*cumulative_cleavages_)[cterm_allowed_cleavages[cterm_idx]-1])
         ){
        break;
      }
      
      if (cterm_allowed_cleavages[cterm_idx] 
            <= nterm_allowed_cleavages[nterm_idx]){
        continue;
      }

      // check our length constraint
      int length = 
       cterm_allowed_cleavages[cterm_idx] - nterm_allowed_cleavages[nterm_idx];

      if (length < get_peptide_constraint_min_length(constraint)){
        continue;
      } else if (length > get_peptide_constraint_max_length(constraint)){
        break;
      } else if (no_new_cterm_cleavage_start){
        next_cterm_cleavage_start = cterm_idx;
        no_new_cterm_cleavage_start = FALSE;
      }
     
      // check our mass constraint
      FLOAT_T peptide_mass = calculateSubsequenceMass(mass_array_, 
          nterm_allowed_cleavages[nterm_idx], length);

      if ((get_peptide_constraint_min_mass(constraint) <= peptide_mass) && 
          (peptide_mass <= get_peptide_constraint_max_mass(constraint))){ 

        // we have found a peptide
        nterm_cleavage_positions_->push_back(nterm_allowed_cleavages[nterm_idx] + 1);

        peptide_lengths_->push_back(length);
        peptide_masses_->push_back(peptide_mass);
        carp(CARP_DETAILED_DEBUG, 
            "New pep: %i (%i)", nterm_allowed_cleavages[nterm_idx], length);

        num_cleavages_++;
      }
    }
    previous_cterm_cleavage_start = next_cterm_cleavage_start;

  }
}

/**
 * Creates the data structures in the protein_peptide_iterator object necessary
 * for creating peptide objects.
 * - mass_array - cumulative distribution of masses. used to determine 
 *     the mass of any peptide subsequence.
 * - nterm_cleavage_positions - the nterm cleavage positions of the 
 *     peptides that satisfy the protein_peptide_iterator contraints
 * - peptide_lengths - the lengths of the peptides that satisfy the constraints
 * - cumulative_cleavages - cumulative distribution of cleavage positions
 *    used to determine if a cleavage location has been skipped
 */
void ProteinPeptideIterator::prepare()
{
  prepareMc(get_boolean_parameter("missed-cleavages"));
}

void ProteinPeptideIterator::prepareMc(
    bool missed_cleavages)
{
  Protein* protein = protein_;
  MASS_TYPE_T mass_type = get_peptide_constraint_mass_type(peptide_constraint_);
  double* mass_array = (double*)mycalloc(protein->getLength()+1, sizeof(double));

  //  PEPTIDE_TYPE_T pep_type = get_peptide_type_parameter("cleavages");
  ENZYME_T enzyme = get_peptide_constraint_enzyme(peptide_constraint_);
  FLOAT_T mass_h2o = MASS_H2O_AVERAGE;

  // set correct H2O mass
  if(mass_type == MONO){
    mass_h2o = MASS_H2O_MONO;
  }
  
  // initialize mass matrix and enzyme cleavage positions
  int* cleavage_positions = (int*) mycalloc(protein->getLength()+1, sizeof(int));
  int* non_cleavage_positions = (int*)mycalloc(protein->getLength()+1, sizeof(int));
  int* all_positions = (int*) mycalloc(protein->getLength()+1, sizeof(int));

  // initialize first value in all array except non_cleavage_positions
  unsigned int start_idx = 0;
  mass_array[start_idx] = 0.0;
  int cleavage_position_idx = 0;
  int non_cleavage_position_idx = 0;
  cleavage_positions[cleavage_position_idx++] = 0;

  // calculate our cleavage positions and masses
  for(start_idx = 1; start_idx < protein->getLength()+1; start_idx++){
    int sequence_idx = start_idx - 1; 
    mass_array[start_idx] = mass_array[start_idx-1] + 
      get_mass_amino_acid(protein->getSequencePointer()[sequence_idx], mass_type);

    // increment cumulative cleavages before we check if current position
    // is a cleavage site because cleavages come *after* the current amino acid
    cumulative_cleavages_->push_back(cleavage_position_idx);

    //if (valid_cleavage_position(protein->sequence + sequence_idx)){ 
    if (validCleavagePosition(protein->getSequencePointer() + sequence_idx, enzyme)){ 
      cleavage_positions[cleavage_position_idx++] = sequence_idx + 1;
    } else {
      non_cleavage_positions[non_cleavage_position_idx++] = sequence_idx + 1;
    }

    all_positions[sequence_idx] = sequence_idx;
  }

  // put in the implicit cleavage at end of protein
  if (cleavage_positions[cleavage_position_idx-1] != (int)protein->getLength()){
    cleavage_positions[cleavage_position_idx++] = protein->getLength(); 
  }

  all_positions[protein->getLength()] = (int)protein->getLength();

  int num_cleavage_positions = cleavage_position_idx;
  int num_non_cleavage_positions = non_cleavage_position_idx;
  mass_array_ = mass_array;

  carp(CARP_DETAILED_DEBUG, "num_cleavage_positions = %i", num_cleavage_positions);

  // now determine the cleavage positions that actually match our constraints

  DIGEST_T digestion = 
    get_peptide_constraint_digest(peptide_constraint_);

  switch (digestion){

  case FULL_DIGEST:
      this->addCleavages(
        cleavage_positions, num_cleavage_positions-1,
        cleavage_positions+1, num_cleavage_positions-1, 
        missed_cleavages);

      break;

  case PARTIAL_DIGEST:
      // add the C-term tryptic cleavage positions.
      this->addCleavages(
        all_positions, protein->getLength(),
        cleavage_positions+1, num_cleavage_positions-1, 
        missed_cleavages);

      // add the N-term tryptic cleavage positions.
      // no +1 below for non_cleavage_positions below 
      // because it does not include sequence beginning. it is *special*
      this->addCleavages(
        cleavage_positions, num_cleavage_positions-1,
        non_cleavage_positions, num_non_cleavage_positions-1,
        missed_cleavages);

      break;

  case NON_SPECIFIC_DIGEST:
      this->addCleavages(
        all_positions, protein->getLength(),
        all_positions+1, protein->getLength(), // len-1?
        TRUE); // for unspecific ends, allow internal cleavage sites
      break;

  case INVALID_DIGEST:
  case NUMBER_DIGEST_TYPES:
    carp(CARP_FATAL, "Invalid digestion type in protein peptide iterator.");
  }

/*
  int idx;
  for (idx=0; idx < iterator->num_cleavages; idx++){
    carp(CARP_DETAILED_DEBUG, "%i->%i", 
         iterator->nterm_cleavage_positions[idx], 
         //iterator->peptide_lengths[idx], 
         //iterator->peptide_lengths[idx], 
       iterator->protein->sequence[iterator->nterm_cleavage_positions[idx]-1]);
  }
*/
  if (num_cleavages_ > 0){
    has_next_ = true;
  } else { 
    has_next_ = false;
  }

  free(cleavage_positions);
  free(non_cleavage_positions);
  free(all_positions);
}

/**
 * \brief Estimate the maximum number of peptides a protein can
 * produce.  Counts the number of subsequences of length
 * min_seq_length, min_seq_length + 1, ..., max_seq_length that can be
 * formed from a protein of the given length.  No enzyme specificity
 * assumed.  
 */
unsigned int ProteinPeptideIterator::countMaxPeptides(
 unsigned int protein_length,   ///< length of protein
 unsigned int min_seq_length,   ///< min peptide length
 unsigned int max_seq_length)  ///< max peptide length
{
  if( max_seq_length > protein_length ){
    max_seq_length = protein_length;
  }

  unsigned int total_peptides = 0;
  for(unsigned int len = min_seq_length; len <= max_seq_length; len++){
    total_peptides += protein_length + 1 - len;
  }
  return total_peptides;
}

/**
 * Instantiates a new peptide_iterator from a protein.
 * \returns a PROTEIN_PEPTIDE_ITERATOR_T object.
 * assumes that the protein is heavy
 */
ProteinPeptideIterator::ProteinPeptideIterator(
  Protein* protein, ///< the protein's peptide to iterate -in
  PEPTIDE_CONSTRAINT_T* peptide_constraint ///< the peptide constraints -in
  )
{

  // initialize iterator
  protein_ = NULL;
  cur_start_ = 0;
  cur_length_ = 1;
  peptide_idx_ = 0;
  peptide_constraint_ = NULL;
  mass_array_ = NULL;
  current_cleavage_idx_ = 0;
  has_next_ = false;

  peptide_idx_ = 0;
  peptide_constraint_ 
    = copy_peptide_constraint_ptr(peptide_constraint);
  cur_start_ = 0; 
  cur_length_ = 1;  
  num_mis_cleavage_ 
    = get_peptide_constraint_num_mis_cleavage(peptide_constraint_);
  protein_ = protein;

  nterm_cleavage_positions_ = new vector<int>();
  peptide_lengths_ = new vector<int>();
  peptide_masses_ = new vector<FLOAT_T>();
  cumulative_cleavages_ = new vector<int>();

  // estimate array size and reserve space to avoid resizing vector
  int max_peptides = countMaxPeptides(protein->getLength(), 
                                        get_int_parameter("min-length"),
                                        get_int_parameter("max-length"));
  nterm_cleavage_positions_->reserve(max_peptides); 
  peptide_lengths_->reserve(max_peptides);
  peptide_masses_->reserve(max_peptides);
  cumulative_cleavages_->reserve(max_peptides);
  num_cleavages_ = 0;

  // prepare the iterator data structures
  prepare();
}


/**
 * Frees an allocated peptide_iterator object.
 */
ProteinPeptideIterator::~ProteinPeptideIterator() 
{
  free_peptide_constraint(peptide_constraint_);
  free(mass_array_); 
  delete nterm_cleavage_positions_; 
  delete peptide_lengths_; 
  delete peptide_masses_; 
  delete cumulative_cleavages_; 
}

/**
 * The basic iterator functions.
 * \returns TRUE if there are additional peptides, FALSE if not.
 */
bool ProteinPeptideIterator::hasNext()
{
  return has_next_;
}

/**
 * \returns The next peptide in the protein, in an unspecified order
 * the Peptide is new heap allocated object, user must free it
 */
PEPTIDE_T* ProteinPeptideIterator::next()
{
  if( !has_next_){
    carp(CARP_DEBUG, "Returning null");
    return NULL;
  }

  int cleavage_idx = current_cleavage_idx_;
  int current_start = (*nterm_cleavage_positions_)[cleavage_idx];
  int current_length = (*peptide_lengths_)[cleavage_idx];
  FLOAT_T peptide_mass = (*peptide_masses_)[cleavage_idx];

  // create new peptide
  PEPTIDE_T* peptide = new_peptide(current_length, peptide_mass, 
                                   protein_, current_start);//, peptide_type);
  // update position of iterator
  ++current_cleavage_idx_;

  // update has_next field
  if (current_cleavage_idx_ == num_cleavages_){
    has_next_ = false;
  } else {
    has_next_ = true;
  }
  return peptide;
}

/**
 *\returns the protein that the iterator was created on
 */
Protein* ProteinPeptideIterator::getProtein()
{
  return protein_;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 2
 * End:
 */