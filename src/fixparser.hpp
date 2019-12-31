#include <string>
#include <cstring>
#include <string_view>
#include <iostream>
#include <vector>
#include <utility>
#include <filesystem>
#include <sstream>
#include <algorithm>
#include <type_traits>
#include <pugixml.hpp>

#define SOH '|'

namespace fixparser {

namespace fs = std::filesystem;
// Defining the necessary types to hold a FIX message
struct Field{
    std::string fieldName_;
    std::string value_;
    std::uint16_t number_{};
    bool isRequired_{};
    bool isComponent_{};
    bool isInGroup_{};
};

struct Group{
    std::string groupName_;
    bool isRequired_{};
    std::vector<Field> grpFields_;
};

struct Component{
    Group group_;
};
struct Message{
    std::string msgName_;
    std::string msgType_;
    std::string msgCat_;
    std::vector<Field> msgFields_;
};

struct Value{
    std::string enumValue_;
    std::string description_;
};

struct Tag{
    std::uint16_t number_{};
    std::string name_;
    std::string type_;
    std::string value_;
    std::vector<Value> tagValues_; // Set of values that a specific Tag can take, this maybe empty for some tags
};

struct Header{
    std::vector<Field> headerFields_;
    Group grp_;
};

struct Body{
    std::vector<Tag> tagValues_;
};

struct Trailer{
    std::vector<Field> trailer_;
};

struct FixMessage{
    Header header_;
    Body body_;
    Trailer trailer_;
    std::string rawMsg_;
};

enum class FixStd : char {
    FIX44
};

struct Error{
    std::string errMsg_;
};
struct ErrorBag{
    std::vector<Error> errors_;

    auto isEmpty() -> bool {
        return errors_.empty();
    }
};

struct Config{

    Config(): pathSrc_("/usr/local/etc"), fixStd_(FixStd::FIX44){}

    template<typename N>
    Config(N&& pathSrc): pathSrc_(std::forward<N>(pathSrc)) {}

    template<typename N>
    Config(N&& pathSrc, FixStd&& fixStd): pathSrc_(std::forward<N>(pathSrc)),
                                          fixStd_(std::move(fixStd)) {}
    auto getPath() const{
        return pathSrc_;
    }

    auto getFixStd() const{
        return fixStd_;
    }

    private:
        std::string pathSrc_;
        FixStd fixStd_;

};

auto operator<<(std::ostream& os, ErrorBag errBag) -> std::ostream&{
    if( errBag.isEmpty() ){
        os << "No errors found" << "\n";
        return os;
    }

     os << "A total of " << errBag.errors_.size() << " error(s) found \n\n";
    for(const auto& err: errBag.errors_ ){
        os << err.errMsg_ << "\n";
    }

    return os;
}

ErrorBag errorBag{};
pugi::xml_document fixSpec;
FixMessage fixMessage;

/**
 * @brief Retrieve the list of errors that occured during parsing
 * @return ErrorBag
 **/
auto getErrors() -> ErrorBag {
     return errorBag;
}

template <typename T,typename S=std::enable_if_t< std::is_convertible_v<T, std::string>, std::string> >
[[nodiscard]] constexpr auto split(T&& str, const char delimiter) noexcept -> std::vector<S> {

  auto strToConvert = std::string(std::forward<T>(str));
  std::vector<S> internal;

  std::stringstream ss( std::forward<S>(strToConvert)); // Turn the string into a stream.
  std::string tok;

  while (getline(ss, tok, delimiter)) {
    internal.emplace_back(tok);
  }

  return internal;
}

template<typename T>
constexpr auto printFieldImpl(T&& field, std::false_type) -> void {

    std::cout <<  field.number_ << "\t\t";
    std::cout << field.name_ << ": " << field.value_ << "\n\n";
}

template<typename T>
constexpr auto printFieldImpl(T&& field, std::true_type) -> void {

    std::cout << field.number_<< "\t\t";
    std::cout << field.fieldName_<< ": " << field.value_ << "\n\n";
}

template<typename T>
constexpr auto printField(T&& field) -> void {
    printFieldImpl( std::forward<T>(field), std::is_same< std::decay_t<T> , Field>() );
}
/**
 * @brief Pretty print a FixMessage
 **/
template<typename T>
constexpr auto prettyPrint(T&& fixMsg) -> void {

    static_assert( std::is_same_v<std::remove_cv_t<std::remove_reference_t<T>>, FixMessage>);

    if( !errorBag.errors_.empty() ){
        std::cerr << "The message contains some errors, please check the FIX specification to get the list of correct fields \n\n";
        std::cerr << errorBag << "\n\n";
        return;
    }
    // Print the header
    std::cout << "HEADER"  << "\n\n";
    for(const auto& field: fixMsg.header_.headerFields_ ){
        printField( field );
    }
    // Print the body
    std::cout << "BODY"  << "\n\n";
    for(const auto& field: fixMsg.body_.tagValues_ ){
        printField( field );
    }
    // Print the trailer
    std::cout << "TRAILER"  << "\n\n";
    for(const auto& field: fixMsg.trailer_.trailer_ ){
        printField( field );
    }
}


/**
 * @brief map a given FIX version to supported one and open the correspoding dictionnary
 * @return true if can open a file with the specified FixStd
 **/

[[nodiscard]] auto mapVersionAndOpenFile(Config& config) noexcept -> bool {
    
    auto mappedVersion = [&config = std::as_const(config)](){
        switch (config.getFixStd()){
            case FixStd::FIX44:
                return "FIX44";
                break;
            default:
                return "FIX44";
                break;
        }
    }();

    // Using the overload of fs::current_path that doesn't throw an exception
    std::error_code ec;
    
    // Changing the current directory to the one set on config.getPath()
    fs::current_path(config.getPath(), ec);

    auto currentDir = fs::current_path(ec); 

    if( ec ){
        return false;
    }

    std::string source = currentDir;
                source += "/fixparser/";
                source += mappedVersion;
                source += ".xml";

    pugi::xml_parse_result result = fixSpec.load_file(source.c_str());

    return static_cast<bool>(result);
}

/**
 * @brief take a vector of string and categorize each element according to the fix spec
 * @return a pair containing the constructed FIX message and an error bag which is empty is no errors found during the process
 **/

template<typename T,typename=std::enable_if_t< !std::is_integral_v<T> > >
[[nodiscard]] constexpr auto categorize(T&& vec, Config& config) noexcept -> std::pair<FixMessage,ErrorBag> {

    FixMessage fixMsg;
    Header fixHeader;
    Body fixBody;
    Trailer fixTrailer;

    auto result = mapVersionAndOpenFile( config );

    if( result ){

        for(auto& tagValue : vec){

            auto tagValueVec = split(tagValue, '=');

            auto headers = fixSpec.child("fix").child("header");
            auto trailers = fixSpec.child("fix").child("trailer");
            auto fields = fixSpec.child("fix").child("fields");

            auto isField = fields.find_node([&tagValueVec](auto& node){
                return
                std::strcmp(node.attribute("number").as_string(), tagValueVec.front().c_str() ) == 0;
            });

            if( isField ){
                 Field f;
                 f.number_ = isField.attribute("number").as_int();
                 f.value_ = tagValueVec.back();

                auto nodeFoundInHeader = headers.find_node([&isField](auto& node){
                    return
                    std::strcmp(node.attribute("name").as_string(), isField.attribute("name").as_string() ) == 0;
                });

                if( nodeFoundInHeader ){
                    if( std::strcmp( nodeFoundInHeader.parent().name(), "group") ){
                        // The node is in a <group> node
                        f.isInGroup_ = true;
                    }

                    f.fieldName_ = static_cast<std::string>(nodeFoundInHeader.attribute("name").as_string());
                    f.isComponent_ = false;
                    f.isRequired_ = nodeFoundInHeader.attribute("required").as_int() != 0x4E ? true : false;

                    fixHeader.headerFields_.emplace_back(f);
                }

                if( !nodeFoundInHeader ){

                    auto nodeFoundInTrailer = trailers.find_node([&isField](auto& node){
                        return
                        std::strcmp(node.attribute("name").as_string(), isField.attribute("name").as_string() ) == 0;
                    });

                    if( nodeFoundInTrailer ){
                        f.fieldName_ = static_cast<std::string>(nodeFoundInTrailer.attribute("name").as_string());
                        f.isComponent_ = false;
                        f.isInGroup_ = false;
                        f.isRequired_ = nodeFoundInTrailer.attribute("required").as_int() != 0x4E ? true : false;

                        fixTrailer.trailer_.emplace_back( f );
                    }else{

                        // The node is in the body in this case so we construct a Tag object
                        // The question about the difference between Tag and Field may arise as this point
                        Tag tag ;

                        tag.name_ = static_cast<std::string>( isField.attribute("name").as_string() );
                        tag.number_ = isField.attribute("number").as_int();
                        tag.type_ = static_cast<std::string>( isField.attribute("type").as_string() );
                        tag.value_ = tagValueVec.back();
                        // If the field has some set of values we retrieve them

                        for( const auto& value: isField.children() ){
                            Value v;
                            v.description_ = static_cast<std::string>( value.attribute("type").as_string() );
                            v.enumValue_  = static_cast<std::string>( value.attribute("enum").as_string() );

                            tag.tagValues_.emplace_back( v );
                        }

                        fixBody.tagValues_.emplace_back( tag );
                    }

                }

            }else{

                // The field is not a correct field, means we didn't found an attribute number=x
                // This results in parsing error
                std::string err = "Field with tag=";
                            err+= tagValueVec.front();
                            err+= " not found";

                errorBag.errors_.emplace_back( Error{std::move(err)} );
            }

        }

        fixMsg.header_ = fixHeader;
        fixMsg.body_ = fixBody;
        fixMsg.trailer_ = fixTrailer;

        return std::make_pair(fixMsg, errorBag);
    }else{

        errorBag.errors_.emplace_back( Error{"Cannot open the FIX spec file."} );
        return std::make_pair( FixMessage{}, errorBag );
    }
}

/**
 * @brief Check the message validity over a Fix specification
 *       if none is specified the FIX44 standard is used
 * @return true if the message is correct false otherwise
 * When it returns false, the list of errors encountered can be get via the getErrors() method
 * and be displayed e.g: std::cout << fixparser::getErrors() << "\n"
*/
template <typename T,typename=std::enable_if_t<std::is_convertible_v<T, std::string> > >
constexpr auto checkMsgValidity(T&& message, Config& config) noexcept -> bool {

    auto splittedMsg = split( message, SOH);

    auto [fixMsg, error] = categorize( splittedMsg, config);

    fixMsg.rawMsg_ = std::forward<T>(message);

    if( !error.isEmpty() ){
        return false;
    }
    
    auto requiredFieldsPresent = hasRequiredFields( fixMsg );

    if( !requiredFieldsPresent ){
        return false;
    }

    auto bodyLengthCorrect = checkBodyLength( fixMsg );

    if( !bodyLengthCorrect ){
        return false;
    }

    auto checkSumCorrect = checkCheckSum( fixMsg );

    if( !checkSumCorrect ){
        return false;
    }

    fixMessage = std::forward<decltype(fixMsg)>(fixMsg);

    return true;
}

template<typename T, typename M>
constexpr auto processComponent(T&&, M&&) -> bool; 

/**
 *  @brief process groups
 *  @return true if the message has the necessary required fields of the group, false otherwise
 **/
template<typename T, typename M>
constexpr auto processGroup(T&& groupNode, M&& message) -> bool{
    
    bool hasRequired{1};

    for(auto& child: groupNode.children()){

            if( std::strcmp("component", child.name() ) == 0 ){
                return processComponent( child.attribute("name").as_string(), std::forward<M>(message) );
            }else{
                auto isFieldPresent = std::find_if( message.body_.tagValues_.begin(),
                                                        message.body_.tagValues_.end(),
                                                        [&child](auto& tag){
                                                            return tag.name_ == child.attribute("name").as_string();
                                                    });

                    if( isFieldPresent == message.body_.tagValues_.end() ){
                        std::string errMsg = "BODY: the tag with name=";
                                    errMsg += child.attribute("name").as_string();
                                    errMsg += " is required";

                        errorBag.errors_.emplace_back( Error{std::move(errMsg)} );
                        hasRequired = false;
            }
        }
    }

    return hasRequired;
}
/**
 * @brief Process the given component for required fields checks
 * @return true if the message has the necessary required fields by the component, false otherwise
 **/
template<typename T, typename M>
constexpr auto processComponent(T&& compName, M&& message) -> bool{
    auto component = fixSpec.child("fix")
                            .child("components")
                            .find_child_by_attribute("component","name", compName );
    
    bool hasRequired{1};

    for(auto& componentField: component.children() ){
        if( std::strcmp( componentField.attribute("required").as_string(), "Y") == 0 ){
            
            if( std::strcmp("component", componentField.name() ) == 0 ) {
                
                return processComponent(componentField.attribute("name").as_string(), std::forward<M>(message));
            }
            else if( std::strcmp("group", componentField.name()) == 0 ) {
                return processGroup(componentField, std::forward<M>(message) );
            }else{

                auto isFieldPresent = std::find_if( message.body_.tagValues_.begin(),
                                                    message.body_.tagValues_.end(),
                                                    [&componentField](auto& tag){
                                                        return tag.name_ == componentField.attribute("name").as_string();
                                                   });

                if( isFieldPresent == message.body_.tagValues_.end() ){
                    std::string errMsg = "BODY: the tag with name=";
                                errMsg += componentField.attribute("name").as_string();
                                errMsg += " is required";

                    errorBag.errors_.emplace_back( Error{std::move(errMsg)} );
                    hasRequired = false;
                }                                   
            }
        }
    }

    return hasRequired;
}
/**
 * @brief Check for required fields in the message
 * @return true if the message has required fields, false otherwise
*/
template <typename T,typename=std::enable_if_t<std::is_same_v<std::decay_t<T>, FixMessage> > >
constexpr auto hasRequiredFields(T&& message) noexcept -> bool{

    auto headerFields = fixSpec.child("fix").child("header").children();
    auto trailerFields = fixSpec.child("fix").child("trailer").children();

    bool hasRequired{true};
    std::string msgType{};

    // Check for required fields in the header
    for(const auto& child: headerFields ){

        if( std::strcmp( child.attribute("required").as_string(), "Y") == 0 ){

            // Check if the field is present in the header
            auto isFieldPresent = std::find_if( message.header_.headerFields_.begin(),
                                                message.header_.headerFields_.end(),
                                                [&child](auto& field){
                                                    return field.fieldName_ == child.attribute("name").as_string();
                                                });

            if( isFieldPresent == message.header_.headerFields_.end() ){
                std::string errMsg = "HEADER: the tag with name=";
                            errMsg += child.attribute("name").as_string();
                            errMsg += " is required";

                errorBag.errors_.emplace_back( Error{std::move(errMsg)} );
                hasRequired = false;
            }else{

            // We retrieve and store the message type at this level
                auto f = *isFieldPresent;

                if( f.number_ == 35 ){
                    msgType = f.value_;
                }
            }

        }
    }

    // Check the required fields in the body
    // NOTE: There are some conditional required fields, not dealing with them as of now
    // NOTE: Some required fields depends on the message type tag 35=MsgType

    auto isCorrectMsgType = fixSpec.child("fix")
                                   .child("messages")
                                   .find_child_by_attribute("message","msgtype", msgType.c_str() );

    if( !isCorrectMsgType ){
        std::string errMsg("The message type is invalid");

        errorBag.errors_.emplace_back( Error{std::move(errMsg)} );
        hasRequired = false;
    }else{

        // We can now check for required fields for the specified message
        // @TODO: Deal with the case of required components

        auto msgField = std::move(isCorrectMsgType);

        for(const auto& child: msgField.children() ){

            if( std::strcmp( child.attribute("required").as_string(), "Y") == 0 ){
                
                // @TODO use tag dispatcher instead of if-else
                // Check if the field is present in the body
            
                if( std::strcmp("component", child.name() ) == 0 ){

                    hasRequired = processComponent( child.attribute("name").as_string(), std::forward<T>(message) );
                                           
                }else if( std::strcmp("group", child.name()) == 0 ){

                    hasRequired = processGroup(child, std::forward<T>(message) );

                }else{

                    auto isFieldPresent = std::find_if( message.body_.tagValues_.begin(),
                                                        message.body_.tagValues_.end(),
                                                        [&child](auto& tag){
                                                            return tag.name_ == child.attribute("name").as_string();
                                                        });

                    if( isFieldPresent == message.body_.tagValues_.end() ){
                        std::string errMsg = "BODY: the tag with name=";
                                    errMsg += child.attribute("name").as_string();
                                    errMsg += " is required";

                        errorBag.errors_.emplace_back( Error{std::move(errMsg)} );
                        hasRequired = false;
                    }

                }

            }
        }

    }

    // Check the required fields for the trailer

    for(const auto& child: trailerFields ){

        if( std::strcmp( child.attribute("required").as_string(), "Y") == 0 ){

            // Check if the field is present in the trailer
            auto isFieldPresent = std::find_if( message.trailer_.trailer_.begin(),
                                                message.trailer_.trailer_.end(),
                                                [&child](auto& field){
                                                    return field.fieldName_ == child.attribute("name").as_string();
                                                });

            if( isFieldPresent == message.trailer_.trailer_.end() ){
                std::string errMsg = "TRAILER: the tag with name=";
                            errMsg += child.attribute("name").as_string();
                            errMsg += " is required";

                errorBag.errors_.emplace_back( Error{std::move(errMsg)} );
                hasRequired = false;
            }

        }
    }

    return hasRequired;
}


/**
 * @brief Check the message body length
 * @return true if the body length is correct false otherwise
*/
template <typename T,typename=std::enable_if_t<std::is_same_v<std::decay_t<T>, FixMessage> > >
constexpr auto checkBodyLength(T&& message) noexcept -> bool {

    int computedLength{};

    for(const auto& msg : message.body_.tagValues_ ){
        computedLength += static_cast<int>( msg.value_.size() ) +
                          static_cast<int>( (std::to_string( msg.number_ )).size() ) + 2; // 2 = the SOH plus the '=' in the message
    }

    for(const auto& msg : message.header_.headerFields_ ){

        // We are excluding the tag 8 which contains the FIX version and the tag 9 which contains the body length
        if( msg.number_ != 9 && msg.number_ != 8 ){
            computedLength += static_cast<int>( msg.value_.size() ) +
                            static_cast<int>( (std::to_string( msg.number_ )).size() ) + 2; // 2 = the SOH plus the '=' in the message
        }

    }

    auto bodyLengthElem = std::find_if( message.header_.headerFields_.begin(),
                                       message.header_.headerFields_.end(),
                                        [](auto& elem){
                                            return elem.number_ == 9;
                                        });

    if( bodyLengthElem != message.header_.headerFields_.end() ){
        auto f = *bodyLengthElem;
        auto areOfEqualLength = std::atoi( f.value_.c_str() ) == computedLength;

        if( !areOfEqualLength ){
            std::string errorMsg = "Message body length mismatch.\nExpected: ";
                        errorMsg += std::to_string(computedLength);
                        errorMsg += "\nGot: ";
                        errorMsg += f.value_;
            errorBag.errors_.emplace_back( Error{ std::move(errorMsg)} );
        }
        return areOfEqualLength;
    }

    return false;
}

/**
 * @brief Check if the message checksum is correct
 * @return true if the checksum is correct false otherwise
 **/

template<typename T,typename=std::enable_if_t<std::is_same_v<std::decay_t<T>, FixMessage> > >
constexpr auto checkCheckSum(T&& message) noexcept -> bool {

    // We assume that the trailer only contains one field which is the checksum
    // While checking the FIX spec we discovered that other fields in the trailer are deprecated
    auto csSize = message.trailer_.trailer_.at(0).value_.size() == 3;
 
    if( !csSize ){
        errorBag.errors_.emplace_back( Error{"The checksum size is invalid. It should be 3"});
        return false;
    }

    uint16_t computedCheckSum{};
    int countSoh{0};

    // The loop is going till the size() - 7, 7=number of characters in the trailing tag of the message
    for(int i{0}; i != message.rawMsg_.size() - 7; ++i){

        if( SOH != message.rawMsg_.at(i) ){
            computedCheckSum += message.rawMsg_.at(i);
        }else{
            ++countSoh;
        }
    }

    computedCheckSum = (computedCheckSum + countSoh)%256;
    auto computedCheckSumStr = std::to_string( computedCheckSum );

    if( computedCheckSumStr.size() != 3 ){
        computedCheckSumStr = "0" + computedCheckSumStr;
    }

    if( computedCheckSumStr != message.trailer_.trailer_.at(0).value_ ){

        std::string errMsg = "The message checksum is invalid.\nExpected: " + computedCheckSumStr + "\nGot: " + message.trailer_.trailer_.at(0).value_+ "\n";
        errorBag.errors_.emplace_back( Error{std::move(errMsg)});
        return false;
    }

    return true;
}

/**
 * @brief Print the message in a human readable way
 **/

template<typename T=void>
constexpr auto fixToHuman() -> void {
    return prettyPrint( fixMessage );
}

}// namespace fixparser
