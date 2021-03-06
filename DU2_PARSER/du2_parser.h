// DU2-ARG
// Pavel Lisa NPRG051 2010/2011
//

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <set>
#include <utility>
#include "parser_value_functor.h"
#include "default_value_functors.h"
#include "known_modifier.h"
#include "unknown_modifier.h"
#include "bad_modifier.h"
#include "unexpected_arguments.h"
#include "conventions.h"

template 
<
	template <typename> class ConventionPolicy, 
	typename ParserCharType, 
	template <typename> class HandleUnknownModifier,
	template <typename> class HandleKnownModifierKeep,
	int HandleBadModifier = BadModifierPolicy::ERROR,
	int HandleUnexpectedArguments = UnexpectedArgumentsPolicy::IGNORE
>
class Parser {
private: 
	// unified string type to support both char and wchar_t
	typedef std::basic_string<ParserCharType> parser_string;
	
	// map modifiers to value functors
	typedef std::map<parser_string, ParserValueFunctor *> functor_map;
	
	// pool of known modifiers
	typedef std::set<parser_string> parser_string_set;
	
	functor_map value_functors;
	parser_string_set known_modifiers;
	parser_string_set modifiers_with_parameters;
	bool expect_arguments;
	std::vector<parser_string> * in_args;
public:
	
	
	Parser(): expect_arguments(false) {
		
	}
	
	/**
	 * Add a modifier with a boolean presence indicator
	 */
	void addPresenceModifier(const parser_string & modifier, bool * presence_trigger) {
		// insert a pair: modifier -> value functor
		// doing so will add a functor that after run, on presence of the modifier 
		// will set the presence trigger to true, otherwise keep it intact
		value_functors.insert(
			std::make_pair(
				ConventionPolicy<parser_string>::makeModifier(modifier),
				new DefaultValueFunctor<bool*>(presence_trigger)
			)
		);
		known_modifiers.insert(ConventionPolicy<parser_string>::makeModifier(modifier));
	}
	
	/**
	 * Add a modifier with a default conversion functor
	 */
	template <typename ResultType>
	void addModifier(const parser_string & modifier, void * target_memory) {
		value_functors.insert(
			std::make_pair(
				ConventionPolicy<parser_string>::makeModifier(modifier), 
				new DefaultValueFunctor<ResultType>(target_memory)
			)
		);
		known_modifiers.insert(ConventionPolicy<parser_string>::makeModifier(modifier));
		modifiers_with_parameters.insert(ConventionPolicy<parser_string>::makeModifier(modifier));
	}
	
	/**
	 * Add a modifier with a custom conversion functor
	 */
	template <typename ResultType, template <typename> class ValueFunctorType>
	void addCustomModifier(const parser_string & modifier, void * target_memory) {
		value_functors.insert(
			std::make_pair(
				ConventionPolicy<parser_string>::makeModifier(modifier), 
				new ValueFunctorType<ResultType>(target_memory)
			)
		);
		known_modifiers.insert(ConventionPolicy<parser_string>::makeModifier(modifier));
		modifiers_with_parameters.insert(ConventionPolicy<parser_string>::makeModifier(modifier));
	}
	
	void putArgumentsInto(std::vector<parser_string> * input_args) {
		expect_arguments = true;
		in_args = input_args;
	}
	
	void run(char ** & argv, int & argc) {
		
		// start from 1st offset, keeping the executable name in args intact
		int pc = 1;
		
		// fill original arguments
		// easily remove arguments later
		// ... and convert to C-array
		std::vector<parser_string> app_args;
		app_args.push_back(argv[0]);
		
		while (pc < argc) {
			// working loop...
			
			if (known_modifiers.find(argv[pc]) != known_modifiers.end()) {
				// must be a modifier ... and must be present in value_functors
				// std::cout << "known, ";
				
				if (modifiers_with_parameters.find(argv[pc]) != modifiers_with_parameters.end()) {
					if (pc != (argc-1)) {
						// parse parameter value
						// std::cout << "normal modifier, ";
						(*value_functors[argv[pc]])(new std::string(argv[pc+1]));
						pc++; // skip one argument, as it was the parameter
					} else {
						// missing required parameter
						// std::cout << "no parameter! ";
						switch (HandleBadModifier) {
							case BadModifierPolicy::THROW_EXCEPTION:
								throw "modifier missing argument";
							case BadModifierPolicy::ERROR:
								std::cerr << "modifier missing argument" << std::endl;
								return;
						}
					}
				} else {
					// modifier without parameter
					// std::cout << "presence modifier, ";
					(*value_functors[argv[pc]])(0);
				}
			} else if (ConventionPolicy<parser_string>::mayBeModifierWithParam(argv[pc])) {
				parser_string possible_modifier_name = ConventionPolicy<parser_string>::extractModifierName(argv[pc]);
				
				if (modifiers_with_parameters.find(possible_modifier_name) != modifiers_with_parameters.end()) {
					parser_string possible_modifier_param = ConventionPolicy<parser_string>::extractModifierParam(argv[pc]);
					
					// std::cout << "found modifier " << argv[pc] << " with param " << possible_modifier_param << std::endl;
					(*value_functors[possible_modifier_name])(&possible_modifier_param);
				} else {
					// std::cout << "suspicious argument, ";
					int to_do = HandleUnknownModifier<parser_string>::deal(app_args, argv[pc]);
					switch (to_do) {
						case UnknownModifierPolicy::THROW_EXCEPTION:
							throw "unknown modifier";
						case UnknownModifierPolicy::ERROR:
							std::cerr << "unknown modifier" << std::endl;
							return;
					}
				}
			} else {
				// unknown modifier or some argument - would require a little extension to the Convention policy class to detect modifiers
				// std::cout << "unknown modifier, ";
				if (expect_arguments) {
					app_args.push_back(argv[pc]);
				} else {
					int to_do = HandleUnknownModifier<parser_string>::deal(app_args, argv[pc]);
					switch (to_do) {
						case UnknownModifierPolicy::THROW_EXCEPTION:
							throw "unknown modifier";
						case UnknownModifierPolicy::ERROR:
							std::cerr << "unknown modifier" << std::endl;
							return;
					}
				}
			}
			
			pc++; 
		}
		
		// printf("%i", pc);
		if (expect_arguments) {
			*in_args = app_args;
		}
	}
	
	~Parser() {
		typedef typename functor_map::iterator f_iter;
		
		f_iter it = value_functors.begin();
		f_iter end = value_functors.end();
		
		for ( ; it != end; ++it) {
			delete (*it).second; // clear memory
		}
	}
};