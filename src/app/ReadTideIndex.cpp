#include "ReadTideIndex.h"

#include "TideMatchSet.h"
#include "io/carp.h"
#include "parameter.h"
#include "app/tide/records_to_vector-inl.h"
#include "app/tide/peptide.h"
#include "util/Params.h"
#include <vector>

using namespace std;

ReadTideIndex::ReadTideIndex() {
}

ReadTideIndex::~ReadTideIndex() {
}

int ReadTideIndex::main(int argc, char** argv) {
  carp(CARP_INFO, "Running read-tide-index...");

  string index_dir = Params::GetString("tide database");
  string peptides_file = index_dir + "/pepix";
  string proteins_file = index_dir + "/protix";
  string auxlocs_file = index_dir + "/auxlocs";

  // Read proteins index file
  carp(CARP_INFO, "Reading proteins...");
  ProteinVec proteins;
  pb::Header protein_header;
  if (!ReadRecordsToVector<pb::Protein, const pb::Protein>(&proteins,
      proteins_file, &protein_header)) {
    carp(CARP_FATAL, "Error reading index (%s)", proteins_file.c_str());
  }
  carp(CARP_DEBUG, "Read %d proteins", proteins.size());

  // Read auxlocs index file
  carp(CARP_INFO, "Reading auxiliary locations...");
  vector<const pb::AuxLocation*> locations;
  if (!ReadRecordsToVector<pb::AuxLocation>(&locations, auxlocs_file)) {
    carp(CARP_FATAL, "Error reading index (%s)", auxlocs_file.c_str());
  }
  carp(CARP_DEBUG, "Read %d auxlocs", locations.size());

  // Read peptides index file
  carp(CARP_INFO, "Reading peptides...");
  pb::Header peptides_header;
  HeadedRecordReader peptide_reader(peptides_file, &peptides_header);
  if (peptides_header.file_type() != pb::Header::PEPTIDES ||
      !peptides_header.has_peptides_header()) {
    carp(CARP_FATAL, "Error reading index (%s)", peptides_file.c_str());
  }

  const pb::Header::PeptidesHeader& pepHeader = peptides_header.peptides_header();
  MassConstants::Init(&pepHeader.mods(), &pepHeader.nterm_mods(), &pepHeader.cterm_mods(),
                      Params::GetDouble("mz-bin-width"),
                      Params::GetDouble("mz-bin-offset"));

  // Set up output file
  string output_file = make_file_path(getName() + ".peptides.txt");
  ofstream* output_stream = create_stream_in_path(
    output_file.c_str(), NULL, Params::GetBool("overwrite"));
  *output_stream << get_column_header(SEQUENCE_COL) << '\t'
                 << get_column_header(PROTEIN_ID_COL) << endl;

  RecordReader* reader = peptide_reader.Reader();
  while (!reader->Done()) {
    // Read peptide
    pb::Peptide pb_peptide;
    reader->Read(&pb_peptide);
    if (Params::GetBool("skip-decoys") && pb_peptide.is_decoy()) {
      continue;
    }
    Peptide peptide(pb_peptide, proteins);

    // Output to file
    *output_stream << peptide.SeqWithMods() << '\t'
                   << proteins[peptide.FirstLocProteinId()]->name();
    if (peptide.HasAuxLocationsIndex()) {
      const pb::AuxLocation* aux_loc = locations[peptide.AuxLocationsIndex()];
      for (int i = 0; i < aux_loc->location_size(); i++) {
        const pb::Protein* protein = proteins[aux_loc->location(i).protein_id()];
        if (protein->has_name()) {
          *output_stream << ';' << protein->name();
        }
      }
    }
    *output_stream << endl;
  }

  output_stream->close();
  delete output_stream;

  return 0;
}

string ReadTideIndex::getName() const {
  return "read-tide-index";
}

string ReadTideIndex::getDescription() const {
  return "Reads an index generated by tide-index and outputs information about "
         "the peptides it contains.";
}

vector<string> ReadTideIndex::getArgs() const {
  string arr[] = {
    "tide database"
  };
  return vector<string>(arr, arr + sizeof(arr) / sizeof(string));
}

vector<string> ReadTideIndex::getOptions() const {
  string arr[] = {
    "skip-decoys"
  };
  return vector<string>(arr, arr + sizeof(arr) / sizeof(string));
}

vector< pair<string, string> > ReadTideIndex::getOutputs() const {
  vector< pair<string, string> > outputs;
  outputs.push_back(make_pair("read-tide-index.peptides.txt",
    "a tab-delimited file containing two columns with headers: the peptide, and "
    "a semicolon-delimited list of IDs of the proteins that peptide occurs in."));
  outputs.push_back(make_pair("read-tide-index.params.txt",
    "a file containing the name and value of all parameters/options for the "
    "current operation. Not all parameters in the file may have been used in "
    "the operation. The resulting file can be used with the --parameter-file "
    "option for other crux programs."));
  outputs.push_back(make_pair("read-tide-index.log.txt",
    "a log file containing a copy of all messages that were printed to the "
    "screen during execution."));
  return outputs;
}

bool ReadTideIndex::needsOutputDirectory() const {
  return true;
}

COMMAND_T ReadTideIndex::getCommand() const {
  return READ_SPECTRUMRECORDS_COMMAND;
}

bool ReadTideIndex::hidden() const {
  return true; 
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 2
 * End:
 */
