#include <system_error>
#include <fstream>

#include "output.h"
#include "slice.h"
#include "log.h"
#include "tools.h"

/* Defines the journal file */
static const std::string journal_file { "index.journal" };

/* Defined where are the files stored before they are moved to the final destination */
static const std::string working_dir { "in_progress" };

static void create_output_directory(std::string& output_directory)
{
  struct stat sb;

  /* check if path exists and is a directory */
  if (stat(output_directory.c_str(), &sb) == 0) {
      if (S_ISDIR (sb.st_mode)) {
          // Is already existing
          LOG(TRACE) << "Output directory already exists: " << output_directory << "'.";    
          return;
      }
      std::string err = "ERROR The output directory path '" + output_directory + "' exists, but the path is not a directory!";
      LOG(ERROR) << err;
      throw std::runtime_error(err);
  }

  if (!tools::filesystem::create_directories(output_directory)) {
    std::string err = tools::strerror("ERROR when creating the output directory '" + output_directory + "'");
    LOG(ERROR) << err;
    throw std::runtime_error(err);
  }
  LOG(TRACE) << "Created output directory: " << output_directory << "'.";    
}

OutputStream::OutputStream( const char* output_file_base, ctrl& c) : 
    tbb::filter(serial_in_order),
    my_output_file_base(output_file_base),
    totcounts(0),
    current_file_size(0),
    file_count(-1),
    control(c),
    current_file(0),
    current_run_number(0),
    journal_name(my_output_file_base + "/" + journal_file)
{
  LOG(TRACE) << "Created output filter at " << static_cast<void*>(this);

  // Create the ouput directory
  std::string output_directory = my_output_file_base + "/" + working_dir;
  create_output_directory(output_directory);
}

static void update_journal(std::string journal_name, uint32_t run_number, uint32_t index)
{
  std::string new_journal_name = journal_name + ".new";

  // Create a new journal file
  std::ofstream journal (new_journal_name);
  if (journal.is_open()) {
    journal << run_number << "\n" << index << "\n";
    journal.close();
  } else {
    LOG(ERROR) << "WARNING: Unable to open journal file";
  }

  // Replace the old journal
  if (rename(new_journal_name.c_str(), journal_name.c_str()) < 0 ) {
    LOG(ERROR) << tools::strerror("Journal file move failed");
  }
}

static bool read_journal(std::string journal_name, uint32_t& run_number, uint32_t& index)
{
    std::ifstream journal (journal_name);
    if (journal.is_open()) {
      journal >> run_number >> index;
      journal.close();
      return true;
    } 
    return false;
}

void* OutputStream::operator()( void* item ) 
{
    Slice& out = *static_cast<Slice*>(item);
    totcounts += out.get_counts();

    if ( control.running.load(std::memory_order_acquire) || control.output_force_write ) {
      if (current_file == NULL || current_file_size > control.max_file_size || current_run_number != control.run_number) {
        open_next_file();
      }
      
      size_t n = fwrite( out.begin(), 1, out.size(), current_file );
      current_file_size += n;      
      if ( n != out.size() ) {
        LOG(ERROR) << "Can't write into output file: Have to write " << out.size() << ", but write returned " << n;
      }
    }

    // If not running and we have a file then close it
    if ( !control.running && current_file != NULL && !control.output_force_write ) {
      close_and_move_current_file();
      file_count = -1;
    }

    out.free();
    return NULL;
}

/*
 * Create a properly formated file name
 * TODO: Change to C++
 */
static std::string format_run_file_stem(uint32_t run_number, int32_t file_count)
{
  char run_order_stem[PATH_MAX];
  snprintf(run_order_stem, sizeof(run_order_stem), "scout_%06d_%06d.dat", run_number, file_count);
  return std::string(run_order_stem);
}

void OutputStream::close_and_move_current_file()
{
  // Close and move current file
  if (current_file) {
    fclose(current_file);
    current_file = NULL;

    std::string run_file          = format_run_file_stem(current_run_number, file_count);
    std::string current_file_name = my_output_file_base + "/" + working_dir + "/" + run_file;
    std::string target_file_name  = my_output_file_base + "/" + run_file;

    LOG(INFO) << "rename: " << current_file_name << " to " << target_file_name;
    if ( rename(current_file_name.c_str(), target_file_name.c_str()) < 0 ) {
      LOG(ERROR) << tools::strerror("File rename failed");
    }

    current_file_size = 0; 
    file_count += 1;
  }
}

void OutputStream::open_next_file() 
{
  close_and_move_current_file();

  // We can change current_run_number only here, after the (previous) file was closed and moved
  if (current_run_number != control.run_number) {
    current_run_number = control.run_number;
    file_count = -1;
  }

  // If this is the first file then check if we have a journal file
  if (file_count < 0) {
    // Set default file index
    file_count = 0;

    uint32_t journal_run_number;
    uint32_t index;

    if (read_journal(journal_name, journal_run_number, index)) {
      LOG(INFO) << "We have journal:";
      LOG(INFO) << "  run_number: " << journal_run_number;
      LOG(INFO) << "  index:      " << index;   
    } else {
      LOG(INFO) << "No journal file.\n";
    }

    LOG(INFO) << "Current run_number: " << current_run_number;
    if (current_run_number == journal_run_number) {
      file_count = index;
    }
    LOG(INFO) << "  using index:      " << file_count;    
  }

  // Create the ouput directory
  std::string output_directory = my_output_file_base + "/" + working_dir;
  create_output_directory(output_directory);

  // Create a new file
  std::string current_filename = output_directory + "/" + format_run_file_stem(current_run_number, file_count);
  current_file = fopen( current_filename.c_str(), "w" );
  if (current_file == NULL) {
    std::string err = tools::strerror("ERROR when creating file '" + current_filename + "'");
    LOG(ERROR) << err;
    throw std::runtime_error(err);
  }

  // Update journal file (with the next index file)
  update_journal(journal_name, current_run_number, file_count+1);
}
