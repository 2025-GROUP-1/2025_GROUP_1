/** @file data_model.h
 *  @brief Doxygen group definition for the Data Model module (no compiled code).
 */

/**
 * @defgroup data_model Data Model
 * @brief Tree model and CAD part management.
 *
 * ModelPart represents a single STL part in the tree, owning the full
 * VTK filter pipeline (STL reader -> clip -> shrink -> mapper -> actor).
 * ModelPartList is the QAbstractItemModel that serves the ModelPart
 * hierarchy to the QTreeView.
 */
