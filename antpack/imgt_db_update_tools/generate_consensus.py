"""Contains the tools needed to convert a stockholm alignment of
all the chain types into a set of .npy arrays and a CONSENSUS.txt
file with consensus sequences for each chain type. The .npy arrays
are used to score new sequences so that they are correctly
aligned and numbered."""
import os
import numpy as np
from Bio.Align import substitution_matrices
from ..constants.allowed_inputs import allowed_aa_list


def write_consensus_file(target_dir, current_dir, alignment_fname):
    """Builds a consensus for the amino acids at each position for each
    chain type. Currently combines all species (it may sometimes be
    desirable to separate species -- will consider this later).
    In target_dir, a file called 'CONSENSUS.txt' is created containing
    the consensus for each chain, while a separate .npy file for each
    chain is saved to the same directory.

    Args:
        target_dir (str): The filepath of the output directory.
        current_dir (str): The filepath of the current directory.
        alignment_fname (str): The name of the alignment file. It should
            already live in target_dir.
    """
    os.chdir(target_dir)
    array_key_dict = {}

    with open(alignment_fname, "r", encoding="utf-8") as fhandle:
        read_now = False
        for line in fhandle:
            if line.startswith("#=GF"):
                read_now = True
                _, chain = line.strip().split()[-1].split("_")
                if chain not in array_key_dict:
                    array_key_dict[chain] = []
            elif line.startswith("#=GC RF"):
                read_now = False
            elif read_now:
                array_key_dict[chain].append(line.strip().split()[-1])

    with open("CONSENSUS.txt", "w+", encoding="utf-8") as fhandle:
        for chain_type, seq_list in array_key_dict.items():
            fhandle.write(f"# CHAIN {chain_type}\n")
            for sequence in seq_list:
                fhandle.write(f"{sequence}\n")
            save_consensus_array(seq_list, chain_type)

    os.chdir(current_dir)


def save_consensus_array(sequences, chain_type):
    """Converts a list of sequences for a specific chain type
    to an array with the score for each possible amino acid substitution at
    each position, including gap penalties. For IMGT (as for other numbering
    schemes), we prefer to place insertions at specific places, so we tailor
    the gap penalties to encourage this. Meanwhile, other positions are
    HIGHLY conserved, so we tailor the penalties to encourage this as well.
    IMGT numbers from 1 so we have to adjust for this."""
    blosum = substitution_matrices.load("BLOSUM62")
    blosum_key = {letter:i for i, letter in enumerate(blosum.alphabet)}

    insertion_positions = {33, 61, 111}
    conserved_positions = {23:["C"], 41:["W"], 104:["C"], 118:["F", "W"]}
    cdrs = {27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
            56, 57, 58, 59, 60, 61, 62, 63, 64, 65,
            105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117}
    key_array = np.zeros((128, 21))
    len_distro = [len(s) for s in sequences]

    if max(len_distro) != min(len_distro):
        raise ValueError("Sequences of different lengths encountered in the MSA")

    for i in range(len_distro[0]):
        position = i + 1
        observed_aas = set()
        for seq in sequences:
            observed_aas.add(seq[i])
        observed_aas = list(observed_aas)

        #First, choose the w2 weight which increases the cost of deviating
        #from expected at conserved positions. Then, choose the gap penalty
        #(column 20 of key array)
        score_weight = 1.0
        if position in conserved_positions:
            score_weight = 5.0
            key_array[i,20] = -55.0
        elif position in cdrs and position not in insertion_positions:
            key_array[i,20] = -26.0
        elif position in insertion_positions:
            key_array[i,20] = -1.0
        elif position in [1,128]:
            key_array[i,20] = 0.0
        else:
            key_array[i,20] = -26.0

        #Next, fill in the scores for other amino acid substitutions. If a conserved
        #residue, use the ones we specify here. Otherwise, use the best possible
        #score given the amino acids observed in the alignments. If the only
        #thing observed in the alignments is gaps, no penalty is applied.
        for j, letter in enumerate(allowed_aa_list):
            letter_blosum_idx = blosum_key[letter]
            if position in conserved_positions:
                score = max([blosum[letter_blosum_idx, blosum_key[k]] for k in
                    conserved_positions[position]])
            else:
                score = max([blosum[letter_blosum_idx, blosum_key[k]] if k != "-" else 0 for k in
                    observed_aas])

            key_array[i,j] = score * score_weight

    np.save(f"CONSENSUS_chain_{chain_type}.npy", key_array)