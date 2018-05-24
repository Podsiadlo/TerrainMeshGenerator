#ifndef _PLUGIN_ANALYSIS_NEW_VAR_ADD_H
#define _PLUGIN_ANALYSIS_NEW_VAR_ADD_H

using namespace clang;
using namespace clang::ast_matchers;
using namespace llvm;
using namespace std;

namespace{

class AddNewVariableHandler : public MatchFinder::MatchCallback {
  private:
    Rewriter &rewriter;
    InfoClass* info;
  public:
    AddNewVariableHandler(Rewriter &rewriter, InfoClass* _info ) :  rewriter(rewriter), info(_info){}
    virtual void run(const MatchFinder::MatchResult &Results) {

      clang::LangOptions LangOpts;
      LangOpts.CPlusPlus = true;
      clang::PrintingPolicy Policy(LangOpts);


      for(auto i : info->newVariables_map){
        if(i.second.size()){
          for(auto j : i.second){
            std::string str_nodeData_struct = "nodeData_struct_" + j.FIELD_NAME + "_" + i.first;
            std::string str_varDecl_getData_src = "varDecl_getData_src_" + j.FIELD_NAME + "_" + i.first;

            auto varDecl_getData_src = Results.Nodes.getNodeAs<clang::VarDecl>(str_varDecl_getData_src);
            auto nodeData_struct = Results.Nodes.getNodeAs<clang::RecordDecl>(str_nodeData_struct); 

            if(varDecl_getData_src){
              std::string str_varName_getData_src = varDecl_getData_src->getNameAsString();

              SourceLocation getData_src_end = varDecl_getData_src->getSourceRange().getEnd().getLocWithOffset(2); 
              std::string newVar_init = "\n" + str_varName_getData_src + "." + j.FIELD_NAME + "=" + j.INIT_VAL + ";\n";
              rewriter.InsertText(getData_src_end, newVar_init, true, true);
              //break;
            }
            if(nodeData_struct){
              nodeData_struct->dumpColor();
              SourceLocation nodeData_loc_end = nodeData_struct->getSourceRange().getEnd().getLocWithOffset(-2);
              findAndReplace(j.FIELD_TYPE, "&", "");
              findAndReplace(j.FIELD_TYPE, "*", "");

              std::string  newVar = "\n" + j.FIELD_TYPE + " " + j.FIELD_NAME + " ;\n";
              rewriter.InsertText(nodeData_loc_end, newVar, true, true);
            }
          }
        }
      }
    }

#if 0
      for(auto i : info->syncFlags_map){
        for(auto j : i.second){
          if(j.IS_RESET){
            string str_memExpr = "memExpr_" + j.VAR_NAME + "_" + i.first;
            string str_resetField = "reset_" + j.VAR_NAME + "_" + i.first;
            string str_resetField_ifStmt = "reset_ifStmt_" + j.VAR_NAME + "_" + i.first;
            string str_main_struct = "main_struct_" + i.first;
            string str_method_operator  = "methodDecl_" + i.first;
            string str_struct_constructor = "constructorDecl_" + i.first;
            string str_struct_constructor_initList = "constructorDeclInitList_" + i.first;
            string str_sdata = "sdata_" + j.VAR_NAME + "_" + i.first;
          string str_resetField_expr = "reset_expr_" + j.VAR_NAME + "_" + i.first;
          string str_do_all = "do_all_" + i.first;

            auto ifStmt = Results.Nodes.getNodeAs<clang::IfStmt>(str_resetField_ifStmt);
            auto callExpr = Results.Nodes.getNodeAs<clang::CallExpr>(str_resetField);
            auto expr = Results.Nodes.getNodeAs<clang::RecordDecl>("record");
            if(ifStmt){
              ifStmt->dumpColor();
              FirstIter_struct_entry first_itr_entry;
              SourceLocation if_loc = ifStmt->getSourceRange().getBegin();
              string str_if_block_arg;
              llvm::raw_string_ostream s_if_block(str_if_block_arg);
              ifStmt->printPretty(s_if_block, 0, Policy);
              string str_if_block = s_if_block.str();
            
              llvm::outs() << "XXXXXXXXXX  : " << str_if_block << "\n";

              string if_block = rewriter.getRewrittenText(ifStmt->getSourceRange());

              //TODO: Assuming if statement has parenthesis. NOT NECESSARY!!
              /**1: remove if statement (order matters) **/
              unsigned len_rm = ifStmt->getSourceRange().getEnd().getRawEncoding() -  if_loc.getRawEncoding() + 1;
              rewriter.RemoveText(if_loc, len_rm);

              /**2: extract operator body without if statement (order matters) **/
              auto method_operator = Results.Nodes.getNodeAs<clang::CXXMethodDecl>(str_method_operator);

              //assuming it has only 1 argument
              assert(method_operator->getNumParams() == 1);
              auto OP_parm_itr = method_operator->param_begin();
              auto first_op_length = (*OP_parm_itr)->getNameAsString().length();
              auto first_end_loc = (*OP_parm_itr)->getSourceRange().getEnd();
              auto first_end = first_end_loc.getRawEncoding();

              //string operator_body = rewriter.getRewrittenText(ifStmt->getSourceRange());
              string operator_body = "void operator(GNode src) {\n\tNodeData& sdata = graph->getData(src); \n" + str_if_block + "\n";
              llvm::outs() << operator_body << "\n";
              //first_itr_entry.OPERATOR_BODY = operator_body;
              /**3: add new if statement (order matters) **/
              /** Adding new condtional outside for loop **/
              auto do_all = Results.Nodes.getNodeAs<clang::Stmt>(str_do_all);
              auto sdata_declStmt = Results.Nodes.getNodeAs<clang::Stmt>(str_sdata);
              SourceLocation do_all_loc_start = do_all->getSourceRange().getBegin();
              string splitOp_call = "splitOP_" + j.FIELD_NAME + "::go(_graph);\n";
              rewriter.InsertText(do_all_loc_start, splitOp_call, true, true);

              /**Add galois accumulator += (work is done) **/
              //string work_done = "\n" + galois_distributed_accumulator_name + "+= 1;\n";
              //rewriter.InsertTextAfter(for_loc_end, work_done);

#if 0
              //TODO: change sdata_loc getLocWithOffset from 2 to total length of statement.
              SourceLocation sdata_loc = sdata_declStmt->getSourceRange().getEnd().getLocWithOffset(2);
              auto binary_op = dyn_cast<clang::BinaryOperator>(ifGreater_2loopTrans->getCond());
              string str_binary_op_arg;
              llvm::raw_string_ostream s_binary_op(str_binary_op_arg);
              binary_op->printPretty(s_binary_op, 0, Policy);
              string str_binary_op = s_binary_op.str();
              findAndReplace(str_binary_op, j.VAR_NAME, info->getData_map[i.first][0].VAR_NAME);
              string new_condition = "\nif(" + str_binary_op + "){\n";
              rewriter.InsertText(sdata_loc, new_condition, true, true);

              auto method_operator_loc = method_operator->getSourceRange().getEnd();
              string extra_paren = "    }\n";
              rewriter.InsertText(method_operator_loc, extra_paren, true, true);
#endif


              /**4: Get main struct arguments (order matters) **/
              auto main_struct = Results.Nodes.getNodeAs<clang::CXXRecordDecl>(str_main_struct);
              stringstream member_variables;
              stringstream constructor_arguments;
              stringstream constructor_initList;
              stringstream arguments;
              for(auto field_itr = main_struct->field_begin(); field_itr != main_struct->field_end(); ++field_itr){
                auto mem_field = std::make_pair(field_itr->getType().getAsString(), field_itr->getNameAsString());
                member_variables << "\n" << mem_field.first << " " << mem_field.second << ";";
                constructor_arguments << " " << mem_field.first << "  _" << mem_field.second  << ",";
                constructor_initList << " " << mem_field.second << "(_" << mem_field.second  << "),";
                arguments << " " << mem_field.second << ", ";
                llvm::outs() << "---> " << mem_field.second << "\n";
                first_itr_entry.MEMBER_FIELD_VEC.push_back(mem_field);
              }

              member_variables << "\n\n";
              string str_constructor_arguments = constructor_arguments.str();
              //To remove the last element i.e extra ","
              str_constructor_arguments.erase(str_constructor_arguments.size() - 1);

              string str_constructor_initList = constructor_initList.str();
              //To remove the last element i.e extra ","
              str_constructor_initList.erase(str_constructor_initList.size() - 1);

              string str_arguments = arguments.str();
              //To remove the last element i.e extra ","
              str_arguments.pop_back();
              findAndReplace(str_arguments, "local_", "");
              findAndReplace(str_arguments, "graph", "&_graph");

              string splitOP_struct = "struct splitOP_" + j.FIELD_NAME + "{";
              splitOP_struct += "\n  " + member_variables.str() + "";
              splitOP_struct += "\n  splitOP_" + j.FIELD_NAME + "(" + str_constructor_arguments + ") : " + str_constructor_initList + "{}";
              splitOP_struct += "\n  static void go(Graph& _graph) {\n\t\t auto& allNodes = _graph.allNodesRange();";
              splitOP_struct += "\n    Galois::do_all(allNodes.begin(), allNodes.end(),\n       splitOP_" + j.FIELD_NAME + "{" + str_arguments + " },";
              splitOP_struct += "\n      Galois::loopname(_graph.get_run_identifier(\"splitOP_" + j.FIELD_NAME + "\").c_str()));}\n"; 

              splitOP_struct += operator_body + "};\n\n"; // End of struct


              SourceLocation main_struct_loc_begin = main_struct->getSourceRange().getBegin();
              rewriter.InsertText(main_struct_loc_begin, splitOP_struct, true, true);
              break;
            }
          }
        }
      }
    }
#endif
};

}//namespace
#endif//_PLUGIN_ANALYSIS_TWO_LOOP_TRANSFORM_H
