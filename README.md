# Smart Cache Cleaner

A C-based Operating Systems project that simulates cache eviction using FIFO, LRU, LFU, and a custom RARE algorithm.

## Overview

Smart Cache Cleaner simulates how a system can select files for eviction when storage space needs to be recovered. It reads file metadata from a CSV dataset, applies a selected eviction policy, skips protected files, and reports the estimated space recovered.

**This project is a simulation only. It does not delete files from your computer.**

## Features

- **FIFO (First-In, First-Out):** Selects the eligible file loaded earliest.
- **LRU (Least Recently Used):** Selects the eligible file with the oldest recorded access order.
- **LFU (Least Frequently Used):** Selects the file with the lowest access count. LRU is used to break ties.
- **RARE (Custom Algorithm):** Uses regeneration confidence, reuse value, urgency, and regret penalty to calculate an eviction score.
- **Protected File Handling:** Skips files marked as protected.
- **Target-Based Eviction:** Selects files until the requested number of bytes is reached or no eligible files remain.
- **CSV Input:** Reads file metadata from a dataset.

## Technologies Used

- C programming language
- GCC compiler
- Ubuntu/Linux terminal
- CSV datasets
- Git and GitHub

## Project Structure

```text
Smart-Cache-Cleaner/
├── pv.c
├── cache_dataset.csv
├── cache_dataset_rare.csv
└── README.md
```

The CSV filenames above are examples; use the filenames actually present in your repository.

## Requirements

- GCC compiler
- Ubuntu or another Linux environment
- A CSV dataset matching the expected column format

## Compilation

Compile the source code using:

```bash
gcc -std=c11 -Wall -Wextra -pedantic pv.c -o pv
```

## Usage

Run the program with three arguments:

```bash
./pv dataset.csv ALGORITHM target_bytes
```

### Examples

```bash
./pv cache_dataset.csv FIFO 10000
./pv cache_dataset.csv LRU 10000
./pv cache_dataset.csv LFU 10000
./pv cache_dataset_rare.csv RARE 10000
```

Replace the dataset filename and target size with your actual values.

## Dataset Format

The first six columns are required for FIFO, LRU, and LFU:

| Column | Description |
|---|---|
| `file_id` | Unique file identifier |
| `file_name` | File name |
| `size_bytes` | File size in bytes |
| `access_count` | Number of accesses |
| `last_access_order` | Recorded access order |
| `protected` | Protection flag: `1` means protected; `0` means eligible |

RARE requires four additional numeric columns:

| Column | Description |
|---|---|
| `regen_confidence` | Confidence that the file can be regenerated |
| `reuse_value` | Value assigned to future reuse |
| `urgency` | Urgency score |
| `regret_penalty` | Penalty associated with evicting the file |

The RARE dataset must contain all ten columns in the order expected by the program.

## RARE Scoring

The custom RARE heuristic calculates:

```text
RARE Score = reuse_value
           + urgency
           + regret_penalty
           - regen_confidence
```

The implementation selects the eligible file with the lowest score first. The score depends on the scale and meaning of the dataset values, so it should be evaluated experimentally.

RARE is a custom heuristic developed for this project, not a standard or universally established cache replacement algorithm.

## Output

The program displays:

- Selected eviction algorithm
- Files selected for eviction
- File sizes and RARE scores
- Total simulated bytes freed
- Whether the requested target was reached

## Safety and Limitations

- The program does not delete or modify real files.
- Results depend on the metadata provided in the CSV.
- CSV parsing supports simple comma-separated fields, not quoted fields containing commas.
- FIFO uses CSV row order as the loading order.
- The RARE heuristic requires appropriate numeric values in its four additional fields.

## Future Enhancements

- Monitor a selected directory for file metadata.
- Record access history automatically.
- Add configurable storage thresholds.
- Compare eviction algorithms using cache hit and miss ratios.
- Improve CSV validation and support more complex CSV formats.
- Add a review process for any future real-file cleanup feature.

## Conclusion

Smart Cache Cleaner demonstrates how different eviction policies can select file-like entries for simulated cleanup. It combines classic FIFO, LRU, and LFU policies with a custom RARE scoring heuristic to explore alternative eviction strategies while respecting protected entries.

## Author

LEA FATHIMA MOOLAYIL 24BDS0408
SARAH FATHIMA 24BCE2728
MOHITHA VELPULA 24BCE2706
