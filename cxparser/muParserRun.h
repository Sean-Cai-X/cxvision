/*
  File: muParserRun.h
  Role: Parser facade or runtime helper layer.
*/

#ifndef MU_PARSERRUN_H
#define MU_PARSERRUN_H
#include "muParser.h"
#include <string>
using namespace std;

#if defined(_WIN32x)
#ifdef _EX
#define   __declspec(dllexport)
#else
#define   __declspec(dllimport)
#endif
#endif

namespace mu
{

	namespace runmu
	{

		/*
		  Role: Lightweight interactive wrapper around mu::Parser for command
		  line style execution, bytecode replay, and manual inspection helpers.
		*/
		class  ParserRun
		{
		public:
			ParserRun(void);
		public:
			~ParserRun(void);

		public:
			mu::Parser  m_parser;

			mu::Parser *GetParser()
			{
				return &m_parser;
			}

			/*
			  Role: Evaluate the current expression stored in the wrapped parser.
			*/
			value_type Eval();
			void SetExpr(string str);
			double GetResult();
			void DefineVar(string str,double *dvalue);
			static double* AddVariable(const char *a_szName,void *pClass);
			void SetVarFactory();

			ParserByteCode::storage_type GetByteCode();
			/*
			  Role: Replay one previously collected bytecode storage block.
			*/
			double RunByteCode(ParserByteCode::storage_type Base);

		private:
			double m_afValBuf[100];
			int m_iVal;
			char m_line[100];
			int m_iget;
			void ListFunction(const mu::Parser  * pParser);
			void ListExprVar(const mu::Parser *parser);
			void ListConst(const mu::Parser *parser);
			void ListVar(const mu::Parser *parser);
			void ListClass(mu::Parser  &Pparser);
			void ListFormula(mu::Parser &parser);
		public:
			void FunTest(void *func);
			void * m_Func;
			void SelfTest();

			/*
			  Role: Execute one interactive shell command against the parser
			  wrapper.
			*/
			bool CommandLine(string szLine);
			void ShowHelp();
			void Calc(const char *pszFormula);
			void Compile(const char *a_szLine);

			void SetStream(std::ostream *a_stream);

		private:
			std::ostream *m_stream;
		};
	}

}

#endif
