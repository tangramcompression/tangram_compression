## Tangram Shader Compression

## Getting Started

Visual Studio 2019 or 2022 is recommended, Tangram compressor is only tested on Windows (Clang or MSVC compiler).

1.Cloning the repository with `git clone https://github.com/tangramcompression/tangram_compression.git`.

2.Configuring the build

```shell
# Create a build directory
mkdir build
cd build

# x86-64 using a Visual Studio solution
cmake -G "Visual Studio 17 2022" ../
```

3.The compiler must be enabled with the -O3/-O2 optimization (otherwise the runtime will be very long).

## Compression Demo

Run the example project, located under the "example" folder in the root directory.

## Benchmark

Run the benchmark project, located under the "benchmark" folder in the root directory. Please note that, due to GitHub's storage limitations, we only provide the benchmarks dataset used for zstd-dict-22 and Tangram.

## Result

| Method            | Compressed Size | CR        | Decompression  |
|-------------------|-----------------|-----------|----------------|
| **Ours**          | **17.2MB**      | **100.5:1** | **264.9ms**    |
| **Zstd-dict-22**  | **157.6MB**     | **11:1**   | 1441.7ms       |
| **Zstd-dict-3**   | 246.4MB         | 7:1       | 1802.7ms       |
| **Zstd-22**       | 391.2MB         | 4.4:1     | 2515.5ms       |
| **Lz4hc-default** | 562.4MB         | 3:1       | **368.2ms**    |
| **Oodle-kraken**  | 435.7MB         | 3.9:1     | 1481.6ms       |

## Source Code Description

The source code is located in the `source` folder. Below is a description of each subfolder:

### `ast_to_gl`
- **Graph Partition**, **Variable Renaming**, **Code Block Generation**

### `core`
- **Logging**, **Multithreading**, and **Common Components**

### `graph`
- **Feature Extraction**, **Graph Clustering**, **Data Dependency Hash Graph Construction**, **Graph Merging**, **Hierarchical Clustering**, **Hierarchical Topological Sorting**, **Graph Visualization**

### `hash_tree`
- **Generate Hash Tree via glslang**, **Serialization**

---

## File Descriptions

### `ast_to_gl`
- **ast_to_glsl.cpp**: Converts abstract syntax tree (AST) to GLSL.
- **graph_partition.cpp**: Graph partitioning.
- **variable_name_manager.cpp**: New variable name management.
- **variable_rename.cpp**: Variable renaming.

### `graph`
- **extra_features.cpp**: Feature extraction.
- **graph_clustering.cpp**: Graph clustering.
- **graph_construct.cpp**: Generates a data dependency hash graph from the hash tree.
- **graph_io.cpp**: Serializes the graph to disk.
- **graph_pair_merge.cpp**: Graph merging.
- **hierarchi_toposort.cpp**: Hierarchical topological sorting.
- **visualize_graph.cpp**: Graph visualization.

### `hash_tree`
- **ast_hash_string.cpp**: Generates a hash string for subtrees to compute hash values.
- **ast_hash_tree.cpp**: Generates a hash tree based on parsing from glslang.
- **ast_node_deepcopy.cpp**: Deep copies nodes for node serialization.
- **node_serialize.cpp**: Abstract syntax tree serialization.


