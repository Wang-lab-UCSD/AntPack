/* Contains the wrapper code for the C++ extension for alignment
 * calculations.
 */

#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/pair.h>

#include <string>
#include "annotator_classes/single_chain_annotator.h"
#include "annotator_classes/paired_chain_annotator.h"
#include "annotator_classes/annotator_base_class.h"
#include "annotator_classes/ig_aligner.h"
#include "annotator_classes/cterm_finder.h"
#include "vj_assignment/vj_match_counter.h"
#include "humanness_calcs/responsibility_calcs.h"
#include "utilities/utilities.h"

namespace nb = nanobind;
using namespace std;

NB_MODULE(antpack_cpp_ext, m){
    nb::class_<AnnotatorBaseClassCpp>(m, "AnnotatorBaseClassCpp")
        .def(nb::init<std::string, std::string>())
        .def("sort_position_codes", &AnnotatorBaseClassCpp::sort_position_codes,
     R"(
        Takes an input list of position codes for a specified scheme and
        sorts them. This is useful since for some schemes (e.g. IMGT)
        sorting is nontrivial, e.g. 112A goes before 112.

        Args:
            position_code_list (list): A list of position codes. If '-'
                is present it is filtered out and is not included in the
                returned list.

        Returns:
            sorted_codes (list): A list of sorted position codes.)")
        .def("build_msa", &AnnotatorBaseClassCpp::build_msa,
     R"(
        Builds a multiple sequence alignment using a list of sequences
        and a corresponding list of tuples output by analyze_seq or
        analyze_seqs (e.g. from PairedChainAnnotator or SingleChainAnnotator).

        Args:
            sequences (list): A list of sequences.
            annotations (list): A list of tuples, each containing (numbering,
                percent_identity, chain_name, error_message). These tuples
                are what you will get as output if you pass sequences to
                the analyze_seq or analyze_seqs methods of SingleChainAnnotator
                or PairedChainAnnotator.

        Returns:
            position_codes (list): A list of position codes from the appropriate numbering
                scheme.
            aligned_seqs (list): A list of strings -- the input sequences all aligned
                to form an MSA.)")
        .def("assign_cdr_labels", &AnnotatorBaseClassCpp::assign_cdr_labels,
     R"(
        Assigns a list of labels "-", "fmwk1", "cdr1", "fmwk2", "cdr2",
        "fmwk3", "cdr3", "fmwk4" to each amino acid in a sequence already
        annotated using the "analyze_seq" or "analyze_seqs" commands. The
        labels indicate which framework region or CDR each amino acid / position
        is in. The labels can be assigned using a different scheme from the
        one used to number, so that you can for example number using aho and
        assign labels using kabat.

        Args:
            alignment (tuple): A tuple containing (numbering,
                percent_identity, chain_name, error_message). This tuple
                is what you will get as output if you pass sequences to
                the analyze_seq method of SingleChainAnnotator
                or PairedChainAnnotator.
            cdr_scheme (str): One of 'aho', 'kabat', 'imgt', 'martin'. Indicates
                the scheme to be used for assigning region labels.

        Returns:
            region_labels (list): A list of strings, each of which is one of
                "fmwk1", "fmwk2", "fmwk3", "fmwk4", "cdr1", "cdr2", "cdr3" or "-".
                This list will be of the same length as the input alignment.)")
        .def("trim_alignment", &AnnotatorBaseClassCpp::trim_alignment,
     R"(
        Takes as input a sequence and a tuple produced by
        analyze_seq and trims off any gap regions at the end
        that result when there are amino acids on either end
        which are not part of the numbered variable region.
        The output from analyze_seq can be fed directly to this
        function.

        Args:
            sequence (str): The input sequence.
            alignment (tuple): A tuple containing (numbering,
                percent_identity, chain_name, error_message). This tuple
                is what you will get as output if you pass sequences to
                the analyze_seq method of SingleChainAnnotator
                or PairedChainAnnotator.

        Returns:
            trimmed_seq (str): The trimmed input sequence.
            trimmed_numbering (list): The first element of the input tuple, the
                numbering, but with all gap regions trimmed off the end.
            exstart (int): The first untrimmed position in the input sequence.
            exend (int): The last untrimmed position in the input sequence.
                The trimmed sequence is sequence[exstart:exend].)");

    nb::class_<SingleChainAnnotatorCpp, AnnotatorBaseClassCpp>(m, "SingleChainAnnotatorCpp")
        .def(nb::init<std::vector<std::string>,
                std::string, bool, std::string>())
        .def("analyze_seq", &SingleChainAnnotatorCpp::analyze_seq,
     R"(
        Numbers and scores a single input sequence. A list of
        outputs from this function can be passed to build_msa
        if desired. The output from this function can also be passed
        to trim_alignment, to assign_cdr_labels and to the VJGeneTool
        as well.

        Args:
            sequence (str): A string which is a sequence containing the usual
                20 amino acids. X is also allowed but should be used sparingly.

        Returns:
            sequence_results (tuple): A tuple of (sequence_numbering, percent_identity,
                chain_name, error_message). If no error was encountered, the error
                message is "". An alignment with low percent identity (e.g. < 0.85)
                may indicate a sequence that is not really an antibody, that contains
                a large deletion, or is not of the selected chain type.)")
        .def("analyze_seqs", &SingleChainAnnotatorCpp::analyze_seqs,
     R"(
        Numbers and scores a list of input sequences. The outputs
        can be passed to other functions like build_msa, trim_alignment,
        assign_cdr_labels and the VJGeneTool if desired.

        Args:
            sequences (list): A list of strings, each of which is a sequence
                containing the usual 20 amino acids. X is also allowed
                (although X should be used with caution; it would be
                impossible to correctly align a sequence consisting mostly
                of X for example).

        Returns:
            sequence_results (list): A list of tuples of (sequence_numbering, percent_identity,
                chain_name, error_message). If no error was encountered, the error
                message is "". An alignment with low percent identity (e.g. < 0.85)
                may indicate a sequence that is not really an antibody, that contains
                a large deletion, or is not of the selected chain type.)");
        //.def("_test_needle_scoring", &SingleChainAnnotatorCpp::_test_needle_scoring);




    nb::class_<PairedChainAnnotatorCpp, AnnotatorBaseClassCpp>(m, "PairedChainAnnotatorCpp")
        .def(nb::init<std::string, std::string>())
        .def("analyze_seq", &PairedChainAnnotatorCpp::analyze_seq,
     R"(
        Extracts and numbers the variable chain regions from a sequence that is
        assumed to contain both a light ('K', 'L') region and a heavy ('H') region.
        The extracted light or heavy chains that are returned can be passed to
        other tools like build_msa, trim_alignment, assign_cdr_labels and the
        VJGeneTool.

        Args:
            sequence (str): A string which is a sequence
                containing the usual 20 amino acids. X is also
                allowed but should be used sparingly.

        Returns:
            heavy_chain_result (tuple): A tuple of (numbering, percent_identity,
                chain_name, error_message). Numbering is the same length as the
                input sequence. A low percent identity or an error message may
                indicate a problem with the input sequence. The error_message is
                "" unless some error occurred.
            light_chain_result (tuple): A tuple of (numbering, percent_identity,
                chain_name, error_message). Numbering is the same length as the input
                sequence. A low percent identity or an error message may indicate a problem
                with the input sequence. The error_message is "" unless some error occurred.)")
        .def("analyze_seqs", &PairedChainAnnotatorCpp::analyze_seqs,
     R"(
        Extracts and numbers the variable chain regions from a list of sequences
        assumed to contain both a light ('K', 'L') region and a heavy ('H') region.
        The extracted light or heavy chains that are returned can be passed to
        other tools like build_msa, trim_alignment, assign_cdr_labels and the
        VJGeneTool.

        Args:
            sequence (str): A string which is a sequence
                containing the usual 20 amino acids. X is also
                allowed but should be used sparingly.

        Returns:
            heavy_chain_results (list): A list of tuples of (numbering, percent_identity,
                chain_name, error_message). Numbering is the same length as the
                corresponding sequence. A low percent identity or an error message may
                indicate a problem with an input sequence. Each error_message is ""
                unless some error occurred for that sequence.
            light_chain_results (list): A list tuples of (numbering, percent_identity,
                chain_name, error_message). Numbering is the same length as the corresponding
                sequence. A low percent identity or an error message may indicate a problem
                with an input sequence. Each error message is "" unless some error
                occurred for that sequence.)");

    nb::class_<VJMatchCounter>(m, "VJMatchCounter")
        .def(nb::init<std::map<std::string, std::vector<std::string>>,
                std::map<std::string, std::vector<std::string>>,
                nb::ndarray<double, nb::shape<22,22>, nb::device::cpu, nb::c_contig>,
                std::string, std::string>() )
        .def("assign_vj_genes", &VJMatchCounter::assign_vj_genes,
     R"(
        Assigns V and J genes for a sequence which has already been
        numbered, preferably by AntPack but potentially by some other
        tool. The database and numbering scheme specified when
        creating the object are used. CAUTION: Make sure the scheme
        used for numbering is the same used for the VJGeneTool.

        Args:
            sequence (str): A sequence containing the usual 20 amino acids -- no gaps.
                X is also allowed but should be used sparingly.
            alignment (tuple): A tuple containing (numbering,
                percent_identity, chain_name, error_message). This tuple
                is what you will get as output if you pass sequences to
                the analyze_seq method of SingleChainAnnotator
                or PairedChainAnnotator.
            species (str): Currently must be one of 'human', 'mouse'.
            mode (str): One of 'identity', 'evalue'. If 'identity' the highest
                percent identity sequence(s) are identified. If 'evalue' the
                lowest e-value (effectively best BLOSUM score) sequence(s)
                are identified.

        Returns:
            v_gene (str): The closest V-gene name(s), as measured by sequence identity.
            j_gene (str): The closest J-gene name(s), as measured by sequence identity.
            v_pident (float): If mode is 'identity', the number of positions at which
                the numbered sequence matches the v-gene divided by the total number of
                non-blank positions in the v-gene. If mode is 'evalue', the best BLOSUM
                score (this can be converted to an e-value). If more than one v-gene
                with the same score is found, multiple
                v-genes are returned as a single string delimited with '_' to separate
                the different v-genes. for the ogrdb database, where multiple names
                have been assigned to the same aa sequence, these multiple names
                are separated by ' ' in the output.
            j_pident (float): If mode is 'identity', the number of positions at which
                the numbered sequence matches the j-gene divided by the total number of
                non-blank positions in the j-gene. If mode is 'evalue', the best BLOSUM
                score (this can be converted to an e-value). If more than one j-gene
                with the same score is found, multiple
                j-genes are returned as a single string delimited with '_' to separate
                the different j-genes. for the ogrdb database, where multiple names
                have been assigned to the same aa sequence, these multiple names
                are separated by ' ' in the output.)") 
        .def("get_vj_gene_sequence", &VJMatchCounter::get_vj_gene_sequence,
     R"(
        Retrieves the amino acid sequence of a specified V or J
        gene, if it is in the latest version of the specified
        database in this version of AntPack. You can use
        assign_vj_genes and this function to see what the VJ
        sequences are (if needed).

        Args:
            vj_gene_name (str): A valid V or J gene name, as generated
                by for example assign_sequence.
            species (str): One of 'human', 'mouse'.

        Returns:
            sequence (str): The amino acid sequence of the V or J gene
                that was requested. If that V or J gene name does not
                match anything, None is returned.)")
        .def("get_seq_lists", &VJMatchCounter::get_seq_lists);

    m.def("getProbsCExt", &getProbsCExt, nb::call_guard<nb::gil_scoped_release>());
    m.def("mask_terminal_deletions", &mask_terminal_deletions, nb::call_guard<nb::gil_scoped_release>());
    m.def("getProbsCExt_masked", &getProbsCExt_masked, nb::call_guard<nb::gil_scoped_release>());
}