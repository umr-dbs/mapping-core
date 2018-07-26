
#ifndef MAPPING_CORE_OGR_SOURCE_DATASETS_H
#define MAPPING_CORE_OGR_SOURCE_DATASETS_H

#include <json/json.h>
#include <ogrsf_frmts.h>

/**
 * This class provides information about available OGR datasets that can be opened with the OGRSourceOperator.
 * The datasets are defined in json files on disk. The location of the json files can be found in the
 * ogrsource.files.path configuration parameter.
 */
class OGRSourceDatasets {
public:
    /**
     * @return vector of names of available OGR dataset
     */
    static std::vector<std::string> getDatasetNames();

    /**
     * Opens the datasets json file and returns its content as a json object. The content is the basic query
     * parameters for opening the corresponding vector file with the OGR Source Operator.
     * @param name: name of the dataset to open.
     * @return json object defining the OGR dataset
     */
    static Json::Value getDatasetDescription(const std::string &name);

    /**
     * Returns a json object containing all the available layers in the vector file on disk.
     * Therefore the vector file is opened with OGR. Additionally for every layer the numeric
     * and textual attributes of that layer are provided.
     * @param dataset_name: name of the dataset to open.
     * @return dataset description json appended by information about available layers.
     */
    static Json::Value getDatasetListing(const std::string &dataset_name);
    /**
     * Check if a key is present in at least one of two json objects.
     * @param layer a json object defining parameters for a layer.
     * @param dataset a json object defining parameters for a dataset.
     * @param key the name of the wanted parameter.
     * @return if the layer or dataset json structures contain a member named key.
     */
    static bool hasJsonParameter(Json::Value &layer, Json::Value &dataset, const std::string &key);
    /**
     * Helper function to retrieve a parameter from one of two json objects.
     * The layer object is prioritized, if the key is present in both json object.
     * If the parameter is not present in both json objects, the default value will be returned
     * @param layer a json object defining parameters for a layer.
     * @param dataset a json object defining parameters for a dataset.
     * @param key the name of the wanted parameter.
     * @param def default value returned if key is not found in layer or dataset
     * @return the value associated to key as json object. The value in layer json is prioritized
     *         should key be member of both jsons.
     */
    static Json::Value getJsonParameterDefault(Json::Value &layer, Json::Value &dataset, const std::string &key, const Json::Value &def);
    /**
     * Same as getJsonParameterDefault, without default parameter. It will return a Null-Json-Object if key is not present.
     * Check if parameter exists with hasJsonParameter before calling.
     * @param layer a json object defining parameters for a layer.
     * @param dataset a json object defining parameters for a dataset.
     * @param key the name of the wanted parameter.
     * @return
     */
    static Json::Value getJsonParameter(Json::Value &layer, Json::Value &dataset, const std::string &key);
};

#endif //MAPPING_CORE_OGR_SOURCE_DATASETS_H
