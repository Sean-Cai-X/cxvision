

#include "Backimagemanager.h"
#include "EasyOcr.h"
#include "FindObject.h"

EasyOCR::EasyOCR()
    : fastmatch(), m_igridw(12), m_igridh(12), m_idebugrectsnum(-1),
      m_idebugfontnum(-1), m_image_thre(110), m_findobj_distance(6),
      m_findobj_searchtype(444), m_findobj_brow(1), m_findobj_minarea(0),
      m_findobj_maxarea(10000), m_findobj_minw(0), m_findobj_maxw(9999),
      m_findobj_minh(0), m_findobj_maxh(9999), m_findobj_bgedge(2),
      m_dmatchthre(0.5), m_icompareobject(3), m_ix_or(0), m_iy_or(0),
      m_ix_and(0), m_iy_and(0), m_idraw_x(-1), m_idraw_y(-1), m_idraw_map(0),
      m_igridwh(12), m_exnum(999), m_findobj_ioffsetx0(0),
      m_findobj_ioffsetx1(0), m_findobj_ioffsety0(0), m_findobj_ioffsety1(0),
      m_imagetype(3), m_resultnodesearchsum(3) {
  setname("OCR");
  fastmatch::setfindnum(1);
  setmatchthre(3);
  fastmatch::setthre(8);
  fastmatch::setcomparegap(2);
  fastmatch::setmethod(0);
  fastmatch::setlinegap(1);
  fastmatch::SetWHgap(1, 1);
  fastmatch::setlinesamplerate(0.002);
  fastmatch::setfilter(0, 0, 10000);
  fastmatch::setfindnum(1);
  fastmatch::setmatchthre(3);
  fastmatch::setfindnum(1);

  clearmodel();
  loadfontmodel();
  levelmodel();
  setlevelstring();

  Shape::setrect(30, 30, 200, 200);
  int icurmodule = BackImageManager::GetCurMode();
  g_pbackimage = BackImageManager::GetBackImage(icurmodule);
  g_pbackobjectimage = BackImageManager::GetBackObjectImage(icurmodule);
  g_pbackfindobject = BackImageManager::Getbackfindobject(icurmodule);

  m_pimagegrid = new Grid;
  m_pimagegrid->setshow(8);
  m_pimagegrid->setgrid(5, 5, 64, 64, 5, 5);
}

EasyOCR::~EasyOCR() {
  delete m_pimagegrid;
  mapclear();
}

void EasyOCR::setrect(int ix, int iy, int iw, int ih) {
  Shape::setrect(ix, iy, iw, ih);
}

std::String EasyOCR::char2string(std::String pchar) {
  if (std::String(pchar) == " ") {
    return std::String("space");
  } else if (std::String(pchar) == "!") {
    return std::String("exclam");
  } else if (std::String(pchar) == "\"") {
    return std::String("quotedbl");
  } else if (std::String(pchar) == "#") {
    return std::String("numbersign");
  } else if (std::String(pchar) == "$") {
    return std::String("dollar");
  } else if (std::String(pchar) == "%") {
    return std::String("percent");
  } else if (std::String(pchar) == "&") {
    return std::String("ampersand");
  } else if (std::String(pchar) == "'") {
    return std::String("apostrophe");
  } else if (std::String(pchar) == "'") {
    return std::String("quoteright");
  } else if (std::String(pchar) == "(") {
    return std::String("parenleft");
  } else if (std::String(pchar) == ")") {
    return std::String("parenright");
  } else if (std::String(pchar) == "*") {
    return std::String("asterisk");
  } else if (std::String(pchar) == "+") {
    return std::String("plus");
  } else if (std::String(pchar) == ",") {
    return std::String("comma");
  } else if (std::String(pchar) == "-") {
    return std::String("minus");
  } else if (std::String(pchar) == ".") {
    return std::String("period");
  } else if (std::String(pchar) == "/") {
    return std::String("slash");
  } else if (std::String(pchar) == "0") {
    return std::String("0");
  } else if (std::String(pchar) == "1") {
    return std::String("1");
  } else if (std::String(pchar) == "2") {
    return std::String("2");
  } else if (std::String(pchar) == "3") {
    return std::String("3");
  } else if (std::String(pchar) == "4") {
    return std::String("4");
  } else if (std::String(pchar) == "5") {
    return std::String("5");
  } else if (std::String(pchar) == "6") {
    return std::String("6");
  } else if (std::String(pchar) == "7") {
    return std::String("7");
  } else if (std::String(pchar) == "8") {
    return std::String("8");
  } else if (std::String(pchar) == "9") {
    return std::String("9");
  } else if (std::String(pchar) == ":") {
    return std::String("colon");
  } else if (std::String(pchar) == ";") {
    return std::String("semicolon");
  } else if (std::String(pchar) == "<") {
    return std::String("less");
  } else if (std::String(pchar) == "=") {
    return std::String("equal");
  } else if (std::String(pchar) == ">") {
    return std::String("greater");
  } else if (std::String(pchar) == "?") {
    return std::String("question");
  } else if (std::String(pchar) == "@") {
    return std::String("at");
  } else if (std::String(pchar) == "A") {
    return std::String("A");
  } else if (std::String(pchar) == "B") {
    return std::String("B");
  } else if (std::String(pchar) == "C") {
    return std::String("C");
  } else if (std::String(pchar) == "D") {
    return std::String("D");
  } else if (std::String(pchar) == "E") {
    return std::String("E");
  } else if (std::String(pchar) == "F") {
    return std::String("F");
  } else if (std::String(pchar) == "G") {
    return std::String("G");
  } else if (std::String(pchar) == "H") {
    return std::String("H");
  } else if (std::String(pchar) == "I") {
    return std::String("I");
  } else if (std::String(pchar) == "J") {
    return std::String("J");
  } else if (std::String(pchar) == "K") {
    return std::String("K");
  } else if (std::String(pchar) == "L") {
    return std::String("L");
  } else if (std::String(pchar) == "M") {
    return std::String("M");
  } else if (std::String(pchar) == "N") {
    return std::String("N");
  } else if (std::String(pchar) == "O") {
    return std::String("O");
  } else if (std::String(pchar) == "P") {
    return std::String("P");
  } else if (std::String(pchar) == "Q") {
    return std::String("Q");
  } else if (std::String(pchar) == "R") {
    return std::String("R");
  } else if (std::String(pchar) == "S") {
    return std::String("S");
  } else if (std::String(pchar) == "T") {
    return std::String("T");
  } else if (std::String(pchar) == "U") {
    return std::String("U");
  } else if (std::String(pchar) == "V") {
    return std::String("V");
  } else if (std::String(pchar) == "W") {
    return std::String("W");
  } else if (std::String(pchar) == "X") {
    return std::String("X");
  } else if (std::String(pchar) == "Y") {
    return std::String("Y");
  } else if (std::String(pchar) == "Z") {
    return std::String("Z");
  } else if (std::String(pchar) == "[") {
    return std::String("bracketleft");
  } else if (std::String(pchar) == "\\") {
    return std::String("backslash");
  } else if (std::String(pchar) == "]") {
    return std::String("bracketright");
  } else if (std::String(pchar) == "^") {
    return std::String("asciicircum");
  } else if (std::String(pchar) == "_") {
    return std::String("underscore");
  } else if (std::String(pchar) == "`") {
    return std::String("grave");
  } else if (std::String(pchar) == "`") {
    return std::String("quoteleft");
  } else if (std::String(pchar) == "a") {
    return std::String("a");
  } else if (std::String(pchar) == "b") {
    return std::String("b");
  } else if (std::String(pchar) == "c") {
    return std::String("c");
  } else if (std::String(pchar) == "d") {
    return std::String("d");
  } else if (std::String(pchar) == "e") {
    return std::String("e");
  } else if (std::String(pchar) == "f") {
    return std::String("f");
  } else if (std::String(pchar) == "g") {
    return std::String("g");
  } else if (std::String(pchar) == "h") {
    return std::String("h");
  } else if (std::String(pchar) == "i") {
    return std::String("i");
  } else if (std::String(pchar) == "j") {
    return std::String("j");
  } else if (std::String(pchar) == "k") {
    return std::String("k");
  } else if (std::String(pchar) == "l") {
    return std::String("l");
  } else if (std::String(pchar) == "m") {
    return std::String("m");
  } else if (std::String(pchar) == "n") {
    return std::String("n");
  } else if (std::String(pchar) == "o") {
    return std::String("o");
  } else if (std::String(pchar) == "p") {
    return std::String("p");
  } else if (std::String(pchar) == "q") {
    return std::String("q");
  } else if (std::String(pchar) == "r") {
    return std::String("r");
  } else if (std::String(pchar) == "s") {
    return std::String("s");
  } else if (std::String(pchar) == "t") {
    return std::String("t");
  } else if (std::String(pchar) == "u") {
    return std::String("u");
  } else if (std::String(pchar) == "v") {
    return std::String("v");
  } else if (std::String(pchar) == "w") {
    return std::String("w");
  } else if (std::String(pchar) == "x") {
    return std::String("x");
  } else if (std::String(pchar) == "y") {
    return std::String("y");
  } else if (std::String(pchar) == "z") {
    return std::String("z");
  } else if (std::String(pchar) == "{") {
    return std::String("braceleft");
  } else if (std::String(pchar) == "|") {
    return std::String("bar");
  } else if (std::String(pchar) == "}") {
    return std::String("braceright");
  } else if (std::String(pchar) == "~") {
    return std::String("asciitilde");
  } else
    return std::String(pchar);
}
std::String EasyOCR::string2char(const std::String &strchar) {
  if (strchar == std::String("space")) {
    return std::String(" ");
  } else if (strchar == std::String("exclam")) {
    return std::String("!");
  } else if (strchar == std::String("quotedbl")) {
    return std::String("\"");
  } else if (strchar == std::String("numbersign")) {
    return std::String("#");
  } else if (strchar == std::String("dollar")) {
    return std::String("$");
  } else if (strchar == std::String("percent")) {
    return std::String("%");
  } else if (strchar == std::String("ampersand")) {
    return std::String("&");
  } else if (strchar == std::String("apostrophe")) {
    return std::String("'");
  } else if (strchar == std::String("quoteright")) {
    return std::String("'");
  } else if (strchar == std::String("parenleft")) {
    return std::String("(");
  } else if (strchar == std::String("parenright")) {
    return std::String(")");
  } else if (strchar == std::String("asterisk")) {
    return std::String("*");
  } else if (strchar == std::String("plus")) {
    return std::String("+");
  } else if (strchar == std::String("comma")) {
    return std::String(",");
  } else if (strchar == std::String("minus")) {
    return std::String("-");
  } else if (strchar == std::String("period")) {
    return std::String(".");
  } else if (strchar == std::String("slash")) {
    return std::String("/");
  } else if (strchar == std::String("0")) {
    return std::String("0");
  } else if (strchar == std::String("1")) {
    return std::String("1");
  } else if (strchar == std::String("2")) {
    return std::String("2");
  } else if (strchar == std::String("3")) {
    return std::String("3");
  } else if (strchar == std::String("4")) {
    return std::String("4");
  } else if (strchar == std::String("5")) {
    return std::String("5");
  } else if (strchar == std::String("6")) {
    return std::String("6");
  } else if (strchar == std::String("7")) {
    return std::String("7");
  } else if (strchar == std::String("8")) {
    return std::String("8");
  } else if (strchar == std::String("9")) {
    return std::String("9");
  } else if (strchar == std::String("colon")) {
    return std::String(":");
  } else if (strchar == std::String("semicolon")) {
    return std::String(";");
  } else if (strchar == std::String("less")) {
    return std::String("<");
  } else if (strchar == std::String("equal")) {
    return std::String("=");
  } else if (strchar == std::String("greater")) {
    return std::String(">");
  } else if (strchar == std::String("question")) {
    return std::String("?");
  } else if (strchar == std::String("at")) {
    return std::String("@");
  } else if (strchar == std::String("A")) {
    return std::String("A");
  } else if (strchar == std::String("B")) {
    return std::String("B");
  } else if (strchar == std::String("C")) {
    return std::String("C");
  } else if (strchar == std::String("D")) {
    return std::String("D");
  } else if (strchar == std::String("E")) {
    return std::String("E");
  } else if (strchar == std::String("F")) {
    return std::String("F");
  } else if (strchar == std::String("G")) {
    return std::String("G");
  } else if (strchar == std::String("H")) {
    return std::String("H");
  } else if (strchar == std::String("I")) {
    return std::String("I");
  } else if (strchar == std::String("J")) {
    return std::String("J");
  } else if (strchar == std::String("K")) {
    return std::String("K");
  } else if (strchar == std::String("L")) {
    return std::String("L");
  } else if (strchar == std::String("M")) {
    return std::String("M");
  } else if (strchar == std::String("N")) {
    return std::String("N");
  } else if (strchar == std::String("O")) {
    return std::String("O");
  } else if (strchar == std::String("P")) {
    return std::String("P");
  } else if (strchar == std::String("Q")) {
    return std::String("Q");
  } else if (strchar == std::String("R")) {
    return std::String("R");
  } else if (strchar == std::String("S")) {
    return std::String("S");
  } else if (strchar == std::String("T")) {
    return std::String("T");
  } else if (strchar == std::String("U")) {
    return std::String("U");
  } else if (strchar == std::String("V")) {
    return std::String("V");
  } else if (strchar == std::String("W")) {
    return std::String("W");
  } else if (strchar == std::String("X")) {
    return std::String("X");
  } else if (strchar == std::String("Y")) {
    return std::String("Y");
  } else if (strchar == std::String("Z")) {
    return std::String("Z");
  } else if (strchar == std::String("bracketleft")) {
    return std::String("[");
  } else if (strchar == std::String("backslash")) {
    return std::String("\\");
  } else if (strchar == std::String("bracketright")) {
    return std::String("]");
  } else if (strchar == std::String("asciicircum")) {
    return std::String("^");
  } else if (strchar == std::String("underscore")) {
    return std::String("_");
  } else if (strchar == std::String("grave")) {
    return std::String("`");
  } else if (strchar == std::String("quoteleft")) {
    return std::String("`");
  } else if (strchar == std::String("a")) {
    return std::String("a");
  } else if (strchar == std::String("b")) {
    return std::String("b");
  } else if (strchar == std::String("c")) {
    return std::String("c");
  } else if (strchar == std::String("d")) {
    return std::String("d");
  } else if (strchar == std::String("e")) {
    return std::String("e");
  } else if (strchar == std::String("f")) {
    return std::String("f");
  } else if (strchar == std::String("g")) {
    return std::String("g");
  } else if (strchar == std::String("h")) {
    return std::String("h");
  } else if (strchar == std::String("i")) {
    return std::String("i");
  } else if (strchar == std::String("j")) {
    return std::String("j");
  } else if (strchar == std::String("k")) {
    return std::String("k");
  } else if (strchar == std::String("l")) {
    return std::String("l");
  } else if (strchar == std::String("m")) {
    return std::String("m");
  } else if (strchar == std::String("n")) {
    return std::String("n");
  } else if (strchar == std::String("o")) {
    return std::String("o");
  } else if (strchar == std::String("p")) {
    return std::String("p");
  } else if (strchar == std::String("q")) {
    return std::String("q");
  } else if (strchar == std::String("r")) {
    return std::String("r");
  } else if (strchar == std::String("s")) {
    return std::String("s");
  } else if (strchar == std::String("t")) {
    return std::String("t");
  } else if (strchar == std::String("u")) {
    return std::String("u");
  } else if (strchar == std::String("v")) {
    return std::String("v");
  } else if (strchar == std::String("w")) {
    return std::String("w");
  } else if (strchar == std::String("x")) {
    return std::String("x");
  } else if (strchar == std::String("y")) {
    return std::String("y");
  } else if (strchar == std::String("z")) {
    return std::String("z");
  } else if (strchar == std::String("braceleft")) {
    return std::String("{");
  } else if (strchar == std::String("bar")) {
    return std::String("|");
  } else if (strchar == std::String("braceright")) {
    return std::String("}");
  } else if (strchar == std::String("asciitilde")) {
    return std::String("~");
  } else
    return std::String(strchar);
}
void EasyOCR::mapclear() {
  int isize = m_pgrids_l72.size();
  for (int i = 0; i < isize; i++) {
    Grid *pgrid = m_pgrids_l72[i];
    if (pgrid)
      delete pgrid;
  }
  m_pgrids_l72.clear();
  isize = m_pgrids_l36.size();
  for (int i = 0; i < isize; i++) {
    Grid *pgrid = m_pgrids_l36[i];
    if (pgrid)
      delete pgrid;
  }
  m_pgrids_l36.clear();

  isize = m_pgrids_l12.size();
  for (int i = 0; i < isize; i++) {
    Grid *pgrid = m_pgrids_l12[i];
    if (pgrid)
      delete pgrid;
  }
  m_pgrids_l12.clear();

  isize = m_pgrids_l6.size();
  for (int i = 0; i < isize; i++) {
    Grid *pgrid = m_pgrids_l6[i];
    if (pgrid)
      delete pgrid;
  }
  m_pgrids_l6.clear();

  isize = m_pgrids_l3.size();
  for (int i = 0; i < isize; i++) {
    Grid *pgrid = m_pgrids_l3[i];
    if (pgrid)
      delete pgrid;
  }
  m_pgrids_l3.clear();
}
void EasyOCR::mapgrid() {
  int ilevel = m_ilevle;
  mapclear();

  int ihnumx = 0;
  int ihnumy = 0;
  int il4size = imagefastmodelsize(4);
  for (int i = 0; i < il4size; i++) {
    Grid *pgrid = new Grid;
    pgrid->setshow(8);
    pgrid->setroi(2050 + ihnumy * 72, 72 * ihnumx + 30, 72, 72);
    if (50 * ihnumx + 50 > 2000) {
      ihnumx = 0;
      ihnumy = ihnumy + 1;
    } else {
      ihnumx = ihnumx + 1;
    }
    pgrid->setgrid(2, 2, 72, 72, 2, 2);
    pgrid->SetModelWH(72, 72);

    SelectModel(4, i);
    pgrid->SetFastModel(*getcurimagemodel());
    m_pgrids_l72.push_back(pgrid);
  }

  ihnumx = 0;
  ihnumy = 0;
  int il3size = imagefastmodelsize(3);
  for (int i = 0; i < il3size; i++) {
    Grid *pgrid = new Grid;
    pgrid->setshow(8);
    pgrid->setroi(1650 + ihnumy * 72, 72 * ihnumx + 30, 72, 72);
    if (50 * ihnumx + 50 > 3000) {
      ihnumx = 0;
      ihnumy = ihnumy + 1;
    } else {
      ihnumx = ihnumx + 1;
    }
    pgrid->setgrid(2, 2, 36, 36, 2, 2);
    pgrid->SetModelWH(36, 36);

    SelectModel(3, i);
    pgrid->SetFastModel(*getcurimagemodel());
    m_pgrids_l36.push_back(pgrid);
  }

  ihnumx = 0;
  ihnumy = 0;
  int il2size = imagefastmodelsize(2);
  for (int i = 0; i < il2size; i++) {
    Grid *pgrid = new Grid;
    pgrid->setshow(8);
    pgrid->setroi(900 + ihnumy * 30, 30 * ihnumx + 30, 20, 20);

    if (50 * ihnumx + 50 > 1500) {
      ihnumx = 0;
      ihnumy = ihnumy + 1;
    } else {
      ihnumx = ihnumx + 1;
    }
    pgrid->setgrid(2, 2, 12, 12, 2, 2);
    pgrid->SetModelWH(12, 12);

    SelectModel(2, i);
    pgrid->SetFastModel(*getcurimagemodel());
    m_pgrids_l12.push_back(pgrid);
  }
  ihnumy = 0;
  ihnumx = 0;
  int il1size = imagefastmodelsize(1);
  for (int i = 0; i < il1size; i++) {
    Grid *pgrid = new Grid;
    pgrid->setshow(8);
    pgrid->setroi(120 + ihnumy * 30, 30 * ihnumx + 30, 20, 20);
    if (30 * ihnumx + 30 > 700) {
      ihnumx = 0;
      ihnumy = ihnumy + 1;
    } else {
      ihnumx = ihnumx + 1;
    }
    pgrid->setgrid(4, 4, 6, 6, 4, 4);
    pgrid->SetModelWH(6, 6);

    SelectModel(1, i);
    pgrid->SetFastModel(*getcurimagemodel());
    m_pgrids_l6.push_back(pgrid);
  }
  int il0size = imagefastmodelsize(0);
  ihnumy = 0;
  ihnumx = 0;
  for (int i = 0; i < il0size; i++) {
    Grid *pgrid = new Grid;
    pgrid->setshow(8);
    pgrid->setroi(20 + ihnumy * 30, 30 * ihnumx + 30, 20, 20);
    if (30 * ihnumx + 30 > 700) {
      ihnumx = 0;
      ihnumy = ihnumy + 1;
    } else {
      ihnumx = ihnumx + 1;
    }
    pgrid->setgrid(8, 8, 3, 3, 8, 8);
    pgrid->SetModelWH(3, 3);

    SelectModel(0, i);
    pgrid->SetFastModel(*getcurimagemodel());
    m_pgrids_l3.push_back(pgrid);
  }

  m_ilevle = ilevel;
}
std::String EasyOCR::filenametoOCRstring(std::String strbase) {
  std::String strkey;
  std::String strkey2;
  std::String strkey3;
  std::String strkey4;
  std::StringList strsplit = strbase.split('_');
  std::String split0, split1, split2, split3, split4;
  if (strsplit.size() > 0)
    split0 = strsplit[0];
  if (strsplit.size() > 1)
    split1 = strsplit[1];
  if (strsplit.size() > 2)
    split2 = strsplit[2];
  if (strsplit.size() > 3)
    split3 = strsplit[3];
  if (strsplit.size() > 4)
    split4 = strsplit[4];
  std::String strshow;
  if (split0 == "") {
    if (split1 == "") {
      strkey = std::String("_");
    } else if (split1 == "1") {
      strkey = std::String("\/");
    } else if (split1 == "2") {
      strkey = std::String("{");
    } else if (split1 == "3") {
      strkey = std::String("}");
    } else if (split1 == "4") {
      strkey = std::String("\\");
    } else if (split1 == "5") {
      strkey = std::String(">");
    } else if (split1 == "6") {
      strkey = std::String("<");
    } else if (split1 == "7") {
      strkey = std::String("@");
    } else if (split1 == "8") {
      strkey = std::String("!");
    } else if (split1 == "9") {
      strkey = std::String(":");
    } else if (split1 == "10") {
      strkey = std::String("\"");
    } else if (split1 == "11") {
      strkey = std::String("\'");
    } else if (split1 == "12") {
      strkey = std::String("~");
    } else if (split1 == "13") {
      strkey = std::String("#");
    } else if (split1 == "14") {
      strkey = std::String("$");
    } else if (split1 == "15") {
      strkey = std::String("%");
    } else if (split1 == "16") {
      strkey = std::String("^");
    } else if (split1 == "17") {
      strkey = std::String("&");
    } else if (split1 == "18") {
      strkey = std::String("*");
    } else if (split1 == "19") {
      strkey = std::String("(");
    } else if (split1 == "20") {
      strkey = std::String(")");
    } else if (split1 == "21") {
      strkey = std::String("-");
    } else if (split1 == "22") {
      strkey = std::String("+");
    } else if (split1 == "23") {
      strkey = std::String("=");
    } else if (split1 == "24") {
      strkey = std::String("?");
    } else if (split1 == "25") {
      strkey = std::String(".");
    } else {
      strkey = string2char(split1);
    }
    strshow = strkey + split2;
  } else {
    if (strsplit.size() <= 2) {
      strkey = string2char(split0);
      strshow = strkey + split1;
    } else if (strsplit.size() == 3) {
      strkey = string2char(split0);
      strkey2 = string2char(split1);
      strshow = strkey + strkey2 + split2;
    } else if (strsplit.size() == 4) {
      strkey = string2char(split0);
      strkey2 = string2char(split1);
      strkey3 = string2char(split2);
      strshow = strkey + strkey2 + strkey3 + split3;
    } else if (strsplit.size() >= 5) {
      strkey = string2char(split0);
      strkey2 = string2char(split1);
      strkey3 = string2char(split2);
      strkey4 = string2char(split3);
      strshow = strkey + strkey2 + strkey3 + strkey4 + split4;
    }
  }
  return strshow;
}
void EasyOCR::loadfontmodel() {
  fastmatch::clearmodels_l12();
  fastmatch::clearmodels_l36();
  fastmatch::clearmodels_l72();

  fastmatch::imagemodesclear_l12();
  fastmatch::imagemodesclear_l36();
  fastmatch::imagemodesclear_l72();

  m_filenamelist_l12.clear();
  m_fontlist_l12.clear();

  m_filenamelist_l36.clear();
  m_fontlist_l36.clear();

  m_filenamelist_l72.clear();
  m_fontlist_l72.clear();

  std::String filenameto = std::String("*.imp");

  std::String path12 = std::String("./model/12x12/");
  std::StringList files12 = QDir(path12).entryList(
      std::StringList(filenameto), QDir::Files | QDir::NoSymLinks);

  std::String path36 = std::String("./model/36x36/");
  std::StringList files36 = QDir(path36).entryList(
      std::StringList(filenameto), QDir::Files | QDir::NoSymLinks);

  std::String path72 = std::String("./model/72x72/");
  std::StringList files72 = QDir(path72).entryList(
      std::StringList(filenameto), QDir::Files | QDir::NoSymLinks);

  for (int i = 0; i < files12.size(); ++i) {
    std::String filename = files12[i];
    QFileInfo afile(filename);
    std::String getfilename_imp =
        afile.completeBaseName() + std::String(".imp");
    std::String strfileimp = path12 + getfilename_imp;
    std::String strbase = afile.completeBaseName();

    fastmatch::addimagemodels_l12(strfileimp.toStdString().c_str());

    m_filenamelist_l12.append(strbase);
    std::String strshow = filenametoOCRstring(strbase);
    m_fontlist_l12.append(strshow);
  }

  for (int i = 0; i < files36.size(); ++i) {
    std::String filename = files36[i];
    QFileInfo afile(filename);
    std::String getfilename_imp =
        afile.completeBaseName() + std::String(".imp");
    std::String strfileimp = path36 + getfilename_imp;
    std::String strbase = afile.completeBaseName();

    fastmatch::addimagemodels_l36(strfileimp.toStdString().c_str());

    m_filenamelist_l36.append(strbase);
    std::String strshow = filenametoORCstring(strbase);
    m_fontlist_l36.append(strshow);
  }

  for (int i = 0; i < files72.size(); ++i) {
    std::String filename = files72[i];
    QFileInfo afile(filename);
    std::String getfilename_imp =
        afile.completeBaseName() + std::String(".imp");
    std::String strfileimp = path72 + getfilename_imp;
    std::String strbase = afile.completeBaseName();

    fastmatch::addimagemodels_l72(strfileimp.toStdString().c_str());

    m_filenamelist_l72.append(strbase);
    std::String strshow = filenametoOCRstring(strbase);
    m_fontlist_l72.append(strshow);
  }
}
void EasyOCR::setimagetype(int itype) { m_imagetype = itype; }
void EasyOCR::setshow(int ishow) { fastmatch::setshow(ishow); }
void EasyOCR::setshowpos(int ix, int iy) {
  m_idraw_x = ix;
  m_idraw_y = iy;
}
void EasyOCR::drawshape(QPainter &painter, QPalette &pal) {
  if (m_idebugfontnum != -1 && m_idebugfontnum < getmodels_l12().size()) {
    getmodels_l12()[m_idebugfontnum].drawshape(painter);
  }
  fastmatch::drawshape(painter, pal);
  if (1 == m_idraw_map || -1 == m_idraw_map) {
    for (int i = 0; i < m_pgrids_l3.size(); i++) {
      m_pgrids_l3[i]->drawshape(painter);
    }

    for (int i = 0; i < m_pgrids_l6.size(); i++) {
      m_pgrids_l6[i]->drawshape(painter);
    }

    for (int i = 0; i < m_pgrids_l12.size(); i++) {
      m_pgrids_l12[i]->drawshape(painter);
    }

    for (int i = 0; i < m_pgrids_l36.size(); i++) {
      m_pgrids_l36[i]->drawshape(painter);
    }

    for (int i = 0; i < m_pgrids_l72.size(); i++) {
      m_pgrids_l72[i]->drawshape(painter);
    }
  } else if (2 == m_idraw_map || -1 == m_idraw_map || -2 == m_idraw_map) {
    if (0 == m_ilevle) {
      int isize = getlevel3_6map().size();
      int jsize = getlevel6_12map().size();
      int ksize = getlevel12_36map().size();
      int lsize = getlevel36_72map().size();
      if (m_idebugfontnum < isize && m_idebugfontnum >= 0) {
        m_pgrids_l3[m_idebugfontnum]->drawshape(painter);
        for (int i = 0; i < isize; i++) {
          if (getlevel3_6map()[i] == m_idebugfontnum) {
            m_pgrids_l6[i]->drawshape(painter);
            for (int j = 0; j < jsize; j++) {
              if (getlevel6_12map()[j] == i) {
                m_pgrids_l12[j]->drawshape(painter);
                for (int k = 0; k < ksize; k++) {
                  if (getlevel12_36map()[k] == j) {
                    m_pgrids_l36[k]->drawshape(painter);
                    for (int l = 0; l < lsize; l++)
                      if (getlevel36_72map()[l] == k)
                        m_pgrids_l72[l]->drawshape(painter);
                  }
                }
              }
            }
          }
        }
      }
    } else if (1 == m_ilevle) {
      int jsize = getlevel6_12map().size();
      int ksize = getlevel12_36map().size();
      int lsize = getlevel36_72map().size();

      int idebugnum1 = getlevel3_6map()[m_idebugfontnum];
      m_pgrids_l3[idebugnum1]->drawshape(painter);

      if (m_idebugfontnum < jsize && m_idebugfontnum >= 0) {
        m_pgrids_l6[m_idebugfontnum]->drawshape(painter);
        for (int j = 0; j < jsize; j++) {
          if (getlevel6_12map()[j] == m_idebugfontnum) {
            m_pgrids_l12[j]->drawshape(painter);
            for (int k = 0; k < ksize; k++) {
              if (getlevel12_36map()[k] == j) {
                m_pgrids_l36[k]->drawshape(painter);
                for (int l = 0; l < lsize; l++) {
                  if (getlevel36_72map()[l] == k)
                    m_pgrids_l72[l]->drawshape(painter);
                }
              }
            }
          }
        }
      }
    } else if (2 == m_ilevle) {
      int jsize = getlevel6_12map().size();
      int ksize = getlevel12_36map().size();
      int lsize = getlevel36_72map().size();

      int idebugnum2 = getlevel6_12map()[m_idebugfontnum];
      m_pgrids_l6[idebugnum2]->drawshape(painter);
      int idebugnum1 = getlevel3_6map()[idebugnum2];
      m_pgrids_l3[idebugnum1]->drawshape(painter);

      if (m_idebugfontnum < jsize && m_idebugfontnum >= 0) {
        int ishownum = getlevel6_12map()[m_idebugfontnum];
        for (int j = 0; j < jsize; j++) {
          if (getlevel6_12map()[j] == ishownum) {
            m_pgrids_l12[j]->drawshape(painter);
            for (int k = 0; k < ksize; k++) {
              if (getlevel12_36map()[k] == j) {
                m_pgrids_l36[k]->drawshape(painter);
                for (int l = 0; l < lsize; l++)
                  if (getlevel36_72map()[l] == k)
                    m_pgrids_l72[l]->drawshape(painter);
              }
            }
          }
        }
      }
    } else if (3 == m_ilevle) {
      int ksize = getlevel12_36map().size();
      int lsize = getlevel36_72map().size();

      int idebugnum3 = getlevel12_36map()[m_idebugfontnum];
      m_pgrids_l12[idebugnum3]->drawshape(painter);
      int idebugnum2 = getlevel6_12map()[idebugnum3];
      m_pgrids_l6[idebugnum2]->drawshape(painter);
      int idebugnum1 = getlevel3_6map()[idebugnum2];
      m_pgrids_l3[idebugnum1]->drawshape(painter);

      if (m_idebugfontnum < ksize && m_idebugfontnum >= 0) {
        int ishownum = getlevel12_36map()[m_idebugfontnum];
        for (int k = 0; k < ksize; k++) {
          if (getlevel6_12map()[k] == ishownum) {
            m_pgrids_l36[k]->drawshape(painter);
            for (int l = 0; l < lsize; l++)
              if (getlevel36_72map()[l] == k)
                m_pgrids_l72[l]->drawshape(painter);
          }
        }
      }
    }

  } else if (3 == m_idraw_map || -2 == m_idraw_map) {
    m_pimagegrid->drawshape(painter);
  } else if (4 == m_idraw_map && m_idraw_x >= 0 && m_idraw_y >= 0) {
    int imodelnum = 0;
    for (int y = 0; y < 800; y += 20) {
      for (int x = 0; x < 600; x += 40) {
        painter.save();
        painter.translate(x + m_idraw_x, y + m_idraw_y);
        Rect arect(0, 0, 40, 40);
        if (imodelnum < m_fontlist_l12.size())
          painter.drawText(arect, Qt::AlignCenter, m_fontlist_l12[imodelnum]);
        imodelnum++;
        painter.restore();
      }
    }
  } else if (103 == m_idraw_map || -100 == m_idraw_map) {
    int iarea = m_reslutnodeslistgrid3x3.size();

    for (int ia = 0; ia < iarea; ia++) {
      if (-1 == m_idebugrectsnum || ia == m_idebugrectsnum) {
        int inodesize = m_reslutnodeslistgrid3x3[ia].getnodes().size();

        for (int in = 0; in < inodesize; in++) {
          int icurnum = m_reslutnodeslistgrid3x3[ia].getnodes()[in].s_inode;
          m_pgrids_l3[icurnum]->drawshape(painter);
        }
      }
    }

  } else if (106 == m_idraw_map || -100 == m_idraw_map) {
    int iarea = m_reslutnodeslistgrid6x6.size();

    for (int ia = 0; ia < iarea; ia++) {
      if (-1 == m_idebugrectsnum || ia == m_idebugrectsnum) {
        int inodesize = m_reslutnodeslistgrid6x6[ia].getnodes().size();
        for (int in = 0; in < inodesize; in++) {
          int icurnum = m_reslutnodeslistgrid6x6[ia].getnodes()[in].s_inode;
          m_pgrids_l6[icurnum]->drawshape(painter);
        }
      }
    }

  } else if (112 == m_idraw_map || -100 == m_idraw_map) {
    int iarea = m_reslutnodeslistgrid12x12.size();

    for (int ia = 0; ia < iarea; ia++) {
      if (-1 == m_idebugrectsnum || ia == m_idebugrectsnum) {
        int inodesize = m_reslutnodeslistgrid12x12[ia].getnodes().size();
        for (int in = 0; in < inodesize; in++) {
          int icurnum = m_reslutnodeslistgrid12x12[ia].getnodes()[in].s_inode;
          m_pgrids_l12[icurnum]->drawshape(painter);
        }
      }
    }

  } else if (-1000 == m_idraw_map) {
    if (m_idebugrectsnum != -1) {
      int icurnum =
          m_reslutnodeslistgrid3x3[m_idebugrectsnum].getnodes()[0].s_inode;
      m_pgrids_l3[icurnum]->drawshape(painter);
      icurnum =
          m_reslutnodeslistgrid6x6[m_idebugrectsnum].getnodes()[0].s_inode;
      m_pgrids_l6[icurnum]->drawshape(painter);
      icurnum =
          m_reslutnodeslistgrid12x12[m_idebugrectsnum].getnodes()[0].s_inode;
      m_pgrids_l12[icurnum]->drawshape(painter);
    }
  }
}
void EasyOCR::setshowmap(int ishow, int ilevel, int idebugfont) {
  m_ilevle = ilevel;
  m_idraw_map = ishow;
  m_idebugfontnum = idebugfont;
}
void EasyOCR::setocrareasnum(int inum) { fastmatch::setmatchrectnum(inum); }
void EasyOCR::setocrareas(int inum, int ix, int iy, int iw, int ih) {
  fastmatch::setmultimatchrect(inum, ix, iy, iw, ih);
}
void EasyOCR::setocrthre(int ithre) { fastmatch::setmatchthre(ithre); }
void EasyOCR::setb2w(int ib2w) { fastmatch::setb2w(ib2w); }

void EasyOCR::setspecshow(int ishow) { fastmatch::setspecshow(ishow); }
void EasyOCR::stringsplit(void *pimage) {
  ImageBase *pgetimage = (ImageBase *)pimage;
  StringSplit(*pgetimage);
}
void EasyOCR::fontsplit(void *pimage) {
  ImageBase *pgetimage = (ImageBase *)pimage;
  FontSplit(*pgetimage);
}
void EasyOCR::exfontsplit(void *pimage) {
  ImageBase *pgetimage = (ImageBase *)pimage;
  ExFontSplit(*pgetimage);
}

void EasyOCR::AreasOCR(void *pimage) {
  ImageBase *pgetimage = (ImageBase *)pimage;
  AreasOCR(*pgetimage);
}
void EasyOCR::setdebug(int idebugrect, int idebugfont) {
  m_idebugrectsnum = idebugrect;
  m_idebugfontnum = idebugfont;
}
void EasyOCR::setsplitimage(int ithre, int ixor, int iyor, int ixand,
                            int iyand) {
  m_image_thre = ithre;
  m_ix_or = ixor;
  m_iy_or = iyor;
  m_ix_and = ixand;
  m_iy_and = iyand;
}
void EasyOCR::setsplitobjectbg(int ibgedge, int ibgmethod) {
  m_findobj_bgedge = ibgedge;
  m_findobj_bgmethod = ibgmethod;
}
void EasyOCR::setsplitobject(int idistance, int isearchtype, int ibrow,
                             int iminarea, int ibgedge) {
  m_findobj_distance = idistance;
  m_findobj_searchtype = isearchtype;
  m_findobj_brow = ibrow;
  m_findobj_minarea = iminarea;
  m_findobj_bgedge = ibgedge;
}
void EasyOCR::setsplitobjectoffset(int ix0, int ix1, int iy0, int iy1) {
  m_findobj_ioffsetx0 = ix0;
  m_findobj_ioffsetx1 = ix1;
  m_findobj_ioffsety0 = iy0;
  m_findobj_ioffsety1 = iy1;
}

void EasyOCR::setsplitgrid(int iw, int ih, int igridnum) {
  g_pbackfindobject->setobjectgrid(iw, ih, igridnum);
}
void EasyOCR::StringSplit(ImageBase &image) {
  Rect arect = Shape::rect();

  g_pbackimage->SetROI(arect.x(), arect.y(), arect.width(), arect.height());
  image.SetROI(arect.x(), arect.y(), arect.width(), arect.height());
  image.SetMode(3);
  image.ROItoROI(g_pbackimage);

  g_pbackimage->ROIImageMoveOrX(m_ix_or);
  g_pbackimage->ROIImageMoveOrY(m_iy_or);
  g_pbackimage->ROIImageMoveAndX(m_ix_and);
  g_pbackimage->ROIImageMoveAndY(m_iy_and);
  g_pbackimage->ROIImageThre(m_image_thre);

  g_pbackfindobject->setrect(arect.x(), arect.y(), arect.width(),
                             arect.height());

  g_pbackfindobject->setdistance(m_findobj_distance);
  g_pbackfindobject->setsearchtype(m_findobj_searchtype);
  g_pbackfindobject->setbrow(m_findobj_brow);

  g_pbackfindobject->setminmaxarea(m_findobj_minarea, m_findobj_maxarea);
  g_pbackfindobject->setminmaxwh(m_findobj_minw, m_findobj_maxw, m_findobj_minh,
                                 m_findobj_maxh);

  g_pbackfindobject->measure(g_pbackimage);

  int ix0 = g_pbackfindobject->getresultx(0);
  int iy0 = g_pbackfindobject->getresulty(0);

  int ih0 = g_pbackfindobject->getresulth(0);
  int iw0 = g_pbackfindobject->getresultw(0);

  ix0 = ix0 - 2 >= 0 ? ix0 - 2 : 0;
  iy0 = iy0 - 2 >= 0 ? iy0 - 2 : 0;

  setrect(ix0, iy0, iw0 + 4, ih0 + 4);
}

void EasyOCR::FontSplit(ImageBase &image) {
  Rect arect = Shape::rect();

  g_pbackimage->SetROI(arect.x(), arect.y(), arect.width(), arect.height());
  image.SetROI(arect.x(), arect.y(), arect.width(), arect.height());
  image.SetMode(3);
  image.ROItoROI(g_pbackimage);

  g_pbackimage->ROIImageMoveOrX(m_ix_or);
  g_pbackimage->ROIImageMoveOrY(m_iy_or);
  g_pbackimage->ROIImageMoveAndX(m_ix_and);
  g_pbackimage->ROIImageMoveAndY(m_iy_and);
  if (0 != m_image_thre)
    g_pbackimage->ROIImageThre(m_image_thre);

  g_pbackfindobject->setrect(arect.x(), arect.y(), arect.width(),
                             arect.height());

  g_pbackfindobject->setdistance(m_findobj_distance);
  g_pbackfindobject->setsearchtype(m_findobj_searchtype);
  g_pbackfindobject->setbrow(m_findobj_brow);

  g_pbackfindobject->setminmaxarea(m_findobj_minarea, m_findobj_maxarea);
  g_pbackfindobject->setminmaxwh(m_findobj_minw, m_findobj_maxw, m_findobj_minh,
                                 m_findobj_maxh);

  g_pbackfindobject->setoffset(m_findobj_ioffsetx0, m_findobj_ioffsetx1,
                               m_findobj_ioffsety0, m_findobj_ioffsety1);

  g_pbackfindobject->measure(g_pbackimage);
  g_pbackfindobject->setbackground(m_findobj_bgedge, m_findobj_bgmethod);

  g_pbackfindobject->resultsrectfilter();
  g_pbackfindobject->objectgrid(&image);
}
void EasyOCR::ExFontSplit(ImageBase &image) {
  Rect arect = Shape::rect();

  g_pbackimage->SetROI(arect.x(), arect.y(), arect.width(), arect.height());
  image.SetROI(arect.x(), arect.y(), arect.width(), arect.height());
  image.SetMode(3);
  image.ROItoROI(g_pbackimage);

  g_pbackimage->ROIImageMoveOrX(m_ix_or);
  g_pbackimage->ROIImageMoveOrY(m_iy_or);
  g_pbackimage->ROIImageMoveAndX(m_ix_and);
  g_pbackimage->ROIImageMoveAndY(m_iy_and);
  if (0 != m_image_thre)
    g_pbackimage->ROIImageThre(m_image_thre);
  g_pbackfindobject->setrect(arect.x(), arect.y(), arect.width(),
                             arect.height());

  g_pbackfindobject->setdistance(m_findobj_distance);
  g_pbackfindobject->setsearchtype(m_findobj_searchtype);
  g_pbackfindobject->setbrow(m_findobj_brow);

  g_pbackfindobject->setminmaxarea(m_findobj_minarea, m_findobj_maxarea);
  g_pbackfindobject->setminmaxwh(m_findobj_minw, m_findobj_maxw, m_findobj_minh,
                                 m_findobj_maxh);

  g_pbackfindobject->setoffset(m_findobj_ioffsetx0, m_findobj_ioffsetx1,
                               m_findobj_ioffsety0, m_findobj_ioffsety1);

  g_pbackfindobject->measure(g_pbackimage);
  g_pbackfindobject->setbackground(m_findobj_bgedge, m_findobj_bgmethod);

  g_pbackfindobject->objectgrid(&image);
}

void EasyOCR::AreasOCR(ImageBase &image) {
  RectsShape arects = fastmatch::getmatchrects();

  m_resultstrlist.clear();
  m_resultstring.clear();

  int iareasnum = arects.size();
  if (iareasnum <= 0)
    return;

  for (int ia = 0; ia < iareasnum; ia++) {
    if (-1 == m_idebugrectsnum || ia == m_idebugrectsnum) {
      int isize = m_fontlist_l12.size();
      int ix = arects.getrect(ia).x();
      int iy = arects.getrect(ia).y();
      int iw = arects.getrect(ia).width();
      int ih = arects.getrect(ia).height();
      fastmatch::setmatchrect(ix, iy, iw, ih);
      double dmaxvalue = 0;
      int iresultfont = 0;
      for (int i = 0; i < isize; i++) {
        if (-1 == m_idebugfontnum || i == m_idebugfontnum) {
          fastmatch::modelstocurrent_l12(i);
          fastmatch::imagemodelstocurrent_l12(i);
          fastmatch::Match(image);
          double dvalue = fastmatch::getmaxresult();
          double dimagevalue = 0;
          if (dvalue > 0.5) {
            fastmatch::imagematch(-1, 1);
            dimagevalue = fastmatch::getimagemodelreslut();
            if (dimagevalue > dmaxvalue) {
              dmaxvalue = dimagevalue;
              iresultfont = i;
            }
            if (-1 == m_idebugfontnum && 100 == dimagevalue)
              break;
          }
        }
      }
      m_resultstrlist.push_back(m_fontlist_l12[iresultfont]);
      m_resultstring.append(m_fontlist_l12[iresultfont]);
    }
  }
  Shape::setname(m_resultstring.toStdString().c_str());
}
void EasyOCR::imagemodelshow() { fastmatch::imagemodelshow(); }

void EasyOCR::imagematchshow() { fastmatch::imagematchshow(); }
void EasyOCR::imagecompareshow(int itype) {
  fastmatch::imagemodelcompareshow(itype);
}
void EasyOCR::autolearn(const char *pfilename) {
  Rect arect = g_pbackfindobject->getgrid(m_idebugrectsnum);
  int ix = arect.x();
  int iy = arect.y();
  int iw = arect.width();
  int ih = arect.height();

  Rect arectrecover = Shape::rect();

  fastmatch::setmatchrect(ix, iy, iw, ih);
  findline::setrect(ix, iy, iw, ih);
  fastmatch::setthre(8);
  fastmatch::setcomparegap(2);
  fastmatch::setmethod(0);
  fastmatch::setlinegap(1);
  fastmatch::SetWHgap(1, 1);
  fastmatch::setlinesamplerate(0.002);
  fastmatch::setfilter(0, 0, 10000);
  fastmatch::learn(g_pbackobjectimage);
  fastmatch::setmatchrect(ix, iy, iw, ih);
  fastmatch::setfindnum(1);
  fastmatch::setmatchthre(3);
  fastmatch::setfindnum(1);
  fastmatch::setmatchthre(3);
  fastmatch::match(g_pbackobjectimage);
  fastmatch::imagelearn(-1, 1);
  std::String strfilename = pfilename;

  QRegExp rxnum("(\\d+)");
  std::StringList listother = strfilename.split(rxnum);

  QRegExp rxother("(\\D+)");
  std::StringList listnum = strfilename.split(rxother);

  std::String strkey = strfilename;
  std::String strkey2;
  std::String strkey3;
  std::String strlast;
  if (listother.size() > 1) {
    std::String qstr = listother[0];
    if (qstr != "" && qstr.size() == 1)
      strkey = qstr;
    else if (qstr.size() == 2) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
    } else if (qstr.size() == 3) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
      strkey3 = qstr.mid(2, 1);
    }

    if (listnum.size() < 2) {
      strkey = listnum[0].mid(0, 1);
      strlast = listnum[0].mid(1);
    } else if (listnum[1] != "")
      strlast = listnum[1];
  }
  std::String strbase;
  std::String charstring = char2string(strkey);
  if (!strkey2.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey2);
  }
  if (!strkey3.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey3);
  }
  strbase = charstring;
  if (0) {
    if (strkey == std::String("\/")) {
      strbase = std::String("_1");
    } else if (strkey == std::String("{")) {
      strbase = std::String("_2");
    } else if (strkey == std::String("}")) {
      strbase = std::String("_3");
    } else if (strkey == std::String("\\")) {
      strbase = std::String("_4");
    } else if (strkey == std::String(">")) {
      strbase = std::String("_5");
    } else if (strkey == std::String("<")) {
      strbase = std::String("_6");
    } else if (strkey == std::String("@")) {
      strbase = std::String("_7");
    } else if (strkey == std::String("!")) {
      strbase = std::String("_8");
    } else if (strkey == std::String(":")) {
      strbase = std::String("_9");
    } else if (strkey == std::String("\"")) {
      strbase = std::String("_10");
    } else if (strkey == std::String("\'")) {
      strbase = std::String("_11");
    } else if (strkey == std::String("~")) {
      strbase = std::String("_12");
    } else if (strkey == std::String("#")) {
      strbase = std::String("_13");
    } else if (strkey == std::String("$")) {
      strbase = std::String("_14");
    } else if (strkey == std::String("%")) {
      strbase = std::String("_15");
    } else if (strkey == std::String("^")) {
      strbase = std::String("_16");
    } else if (strkey == std::String("&")) {
      strbase = std::String("_17");
    } else if (strkey == std::String("*")) {
      strbase = std::String("_18");
    } else if (strkey == std::String("(")) {
      strbase = std::String("_19");
    } else if (strkey == std::String(")")) {
      strbase = std::String("_20");
    } else if (strkey == std::String("-")) {
      strbase = std::String("_21");
    } else if (strkey == std::String("+")) {
      strbase = std::String("_22");
    } else if (strkey == std::String("=")) {
      strbase = std::String("_23");
    } else if (strkey == std::String("?")) {
      strbase = std::String("_24");
    } else if (strkey == std::String(".")) {
      strbase = std::String("_25");
    } else {
      strbase = strkey;
    }
  }
  std::String qstrsavepat = std::String("./model/12x12/") + strbase +
                            std::String("_") + strlast + std::String(".pat");
  std::String qstrsaveimp = std::String("./model/12x12/") + strbase +
                            std::String("_") + strlast + std::String(".imp");
  std::String qstrbase = strbase + std::String("_") + strlast;

  fastmatch::savefastimagemodel(qstrsaveimp.toStdString().c_str());
  fastmatch::savefastimagepatmodel(qstrsavepat.toStdString().c_str());

  int ifontsize = m_fontlist_l12.size();
  for (int in = 0; in < ifontsize; in++) {

    std::String qstr = m_fontlist_l12[in];
    if (strfilename == qstr) {
      fastmatch::setcurmodels(in);
      fastmatch::setcurimagemodels(in);
      break;
    }
    if (in == ifontsize - 1) {
      fastmatch::addimagemodels_l12(qstrsaveimp.toStdString().c_str());

      m_filenamelist_l12.append(qstrbase);
      m_fontlist_l12.append(strfilename);
    }
  }

  ix = arectrecover.x();
  iy = arectrecover.y();
  iw = arectrecover.width();
  ih = arectrecover.height();

  Shape::setrect(ix, iy, iw, ih);

  levelmodel();
}
void EasyOCR::autolearnex(const char *pfilename) {
  Rect arect = g_pbackfindobject->getgridex(m_idebugrectsnum);

  int iobjw = g_pbackfindobject->getresultw(m_idebugrectsnum);
  int iobjh = g_pbackfindobject->getresulth(m_idebugrectsnum);
  int imaxlen = iobjw > iobjh ? iobjw : iobjh;
  int igrid = fastmatch::GetRectGridLevel(imaxlen);

  int ix = arect.x();
  int iy = arect.y();
  int iw = arect.width();
  int ih = arect.height();

  Rect arectrecover = Shape::rect();

  fastmatch::setmatchrect(ix, iy, iw, ih);
  findline::setrect(ix, iy, iw, ih);
  fastmatch::setthre(8);
  fastmatch::setcomparegap(2);
  fastmatch::setmethod(0);
  fastmatch::setlinegap(1);
  fastmatch::SetWHgap(1, 1);
  fastmatch::setlinesamplerate(0.002);
  fastmatch::setfilter(0, 0, 10000);
  fastmatch::learn(g_pbackobjectimage);
  fastmatch::setmatchrect(ix, iy, iw, ih);
  fastmatch::setfindnum(1);
  fastmatch::setmatchthre(3);
  fastmatch::setfindnum(1);
  fastmatch::setmatchthre(3);
  fastmatch::match(g_pbackobjectimage);

  fastmatch::imagelearnex(-1, 1, igrid);

  Grid *pgrid = fastmatch::getgrid();
  std::String strgrid = pgrid->GetGridString();

  std::String strfilename = pfilename;

  QRegExp rxnum("(\\d+)");
  std::StringList listother = strfilename.split(rxnum);

  QRegExp rxother("(\\D+)");
  std::StringList listnum = strfilename.split(rxother);

  std::String strkey = strfilename;
  std::String strkey2;
  std::String strkey3;
  std::String strkey4;
  std::String strkey5;
  std::String strlast;
  if (listother.size() > 1) {
    std::String qstr = listother[0];
    if (qstr != "" && qstr.size() == 1)
      strkey = qstr;
    else if (qstr.size() == 2) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
    } else if (qstr.size() == 3) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
      strkey3 = qstr.mid(2, 1);
    } else if (qstr.size() == 4) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
      strkey3 = qstr.mid(2, 1);
      strkey4 = qstr.mid(3, 1);
    } else if (qstr.size() == 5) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
      strkey3 = qstr.mid(2, 1);
      strkey4 = qstr.mid(3, 1);
      strkey5 = qstr.mid(4, 1);
    }

    if (listnum.size() < 2) {
      strkey = listnum[0].mid(0, 1);
      strlast = listnum[0].mid(1);
    } else if (listnum[1] != "")
      strlast = listnum[1];
  }
  std::String strbase;
  std::String charstring = char2string(strkey);
  if (!strkey2.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey2);
  }
  if (!strkey3.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey3);
  }
  if (!strkey4.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey4);
  }
  if (!strkey5.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey5);
  }

  strbase = charstring;
  std::String qstrsavepat = std::String("./model/") + strgrid +
                            std::String("/") + strbase + std::String("_") +
                            strlast + std::String(".pat");
  std::String qstrsaveimp = std::String("./model/") + strgrid +
                            std::String("/") + strbase + std::String("_") +
                            strlast + std::String(".imp");
  std::String qstrbase = strbase + std::String("_") + strlast;
  fastmatch::savefastimagemodel(qstrsaveimp.toStdString().c_str());
  fastmatch::savefastimagepatmodel(qstrsavepat.toStdString().c_str());

  int ifontsize = m_fontlist_l12.size();
  for (int in = 0; in < ifontsize; in++) {
    std::String qstr = m_fontlist_l12[in];
    if (strfilename == qstr) {

      fastmatch::setcurmodels(in);
      fastmatch::setcurimagemodels(in);
      break;
    }
    if (in == ifontsize - 1) {
      fastmatch::addimagemodels_l12(qstrsaveimp.toStdString().c_str());

      m_filenamelist_l12.append(qstrbase);
      m_fontlist_l12.append(strfilename);
    }
  }

  ix = arectrecover.x();
  iy = arectrecover.y();
  iw = arectrecover.width();
  ih = arectrecover.height();

  Shape::setrect(ix, iy, iw, ih);

  levelmodel();
}
void EasyOCR::autolearnobj(const char *pfilename) {
  Rect arect = g_pbackfindobject->getgridex(m_idebugrectsnum);

  int iobjw = g_pbackfindobject->getresultw(m_idebugrectsnum);
  int iobjh = g_pbackfindobject->getresulth(m_idebugrectsnum);
  int imaxlen = iobjw > iobjh ? iobjw : iobjh;
  int igrid_org = fastmatch::GetRectGridLevel(imaxlen);

  int ix = arect.x();
  int iy = arect.y();
  int iw = arect.width();
  int ih = arect.height();

  g_pbackimage->SetROI(ix, iy, iw, ih);
  m_pimagegrid->ROIImagetoModel(*g_pbackimage);
  m_pimagegrid->ZeroModel();

  m_pimagegrid->ReGrid(igrid_org, igrid_org);
  if (0) {
    m_pimagegrid->SetUnit(igrid_org, igrid_org);
    m_pimagegrid->UnitGrid();
  }

  Grid *pgrid = m_pimagegrid;
  std::String strgrid = pgrid->GetGridString();

  std::String strfilename = pfilename;

  QRegExp rxnum("(\\d+)");
  std::StringList listother = strfilename.split(rxnum);

  QRegExp rxother("(\\D+)");
  std::StringList listnum = strfilename.split(rxother);

  std::String strkey = strfilename;
  std::String strkey2;
  std::String strkey3;
  std::String strkey4;
  std::String strkey5;
  std::String strlast;
  if (listother.size() > 1) {
    std::String qstr = listother[0];
    if (qstr != "" && qstr.size() == 1)
      strkey = qstr;
    else if (qstr.size() == 2) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
    } else if (qstr.size() == 3) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
      strkey3 = qstr.mid(2, 1);
    } else if (qstr.size() == 4) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
      strkey3 = qstr.mid(2, 1);
      strkey4 = qstr.mid(3, 1);
    } else if (qstr.size() == 5) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
      strkey3 = qstr.mid(2, 1);
      strkey4 = qstr.mid(3, 1);
      strkey5 = qstr.mid(4, 1);
    }

    if (listnum.size() < 2) {
      strkey = listnum[0].mid(0, 1);
      strlast = listnum[0].mid(1);
    } else if (listnum[1] != "")
      strlast = listnum[1];
  }
  std::String strbase;
  std::String charstring = char2string(strkey);
  if (!strkey2.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey2);
  }
  if (!strkey3.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey3);
  }
  if (!strkey4.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey4);
  }
  if (!strkey5.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey5);
  }

  strbase = charstring;
  std::String qstrsavepat = std::String("./model/") + strgrid +
                            std::String("/") + strbase + std::String("_") +
                            strlast + std::String(".pat");
  std::String qstrsaveimp = std::String("./model/") + strgrid +
                            std::String("/") + strbase + std::String("_") +
                            strlast + std::String(".imp");
  std::String qstrbase = strbase + std::String("_") + strlast;

  bool bsaveok = false;
  int isavenum = 0;
  while (!bsaveok) {
    std::String strlast1 = std::String("%1").arg(isavenum);
    qstrsaveimp = std::String("./model/") + strgrid + std::String("/") +
                  strbase + std::String("_") + strlast1 + std::String(".imp");
    isavenum = isavenum + 1;
    QFileInfo afileinf(qstrsaveimp);
    if (!afileinf.exists()) {
      m_pimagegrid->savemapmodel(qstrsaveimp.toStdString().c_str());
      bsaveok = true;
    }
  }

  clearmodel();
  loadfontmodel();
  levelmodel();
  setlevelstring();
}

void EasyOCR::setlearngridwh(int igridwh) { m_igridwh = igridwh; }

void EasyOCR::string_exnum(int inum) { m_exnum = inum; }

void EasyOCR::string_autolearnmass(const char *pstring) {
  std::String qstr(pstring);
  int istrnum = qstr.size();
  int igetobj = g_pbackfindobject->getresultobjsnum();
  if (istrnum == igetobj) {
    for (int i = 0; i < istrnum; i++) {
      m_idebugrectsnum = i;
      std::String qchar(pstring[i]);
      std::String qname = qchar + std::String("%1").arg(m_exnum);
      learnmass_36(qname.toStdString().c_str());
    }
    clearmodel();
    loadfontmodel();
    levelmodel();
    setlevelstring();

    mapgrid();
    setspecshow(-1);
  }
}
void EasyOCR::autolearnmass(const char *pfilename) {
  Rect arect = g_pbackfindobject->getgridex(m_idebugrectsnum);

  int iobjw = g_pbackfindobject->getresultw(m_idebugrectsnum);
  int iobjh = g_pbackfindobject->getresulth(m_idebugrectsnum);
  int imaxlen = iobjw > iobjh ? iobjw : iobjh;
  int igrid = fastmatch::GetRectGridLevel(imaxlen);

  int ix = arect.x();
  int iy = arect.y();
  int iw = arect.width();
  int ih = arect.height();
  Rect arectrecover = Shape::rect();

  fastmatch::setmatchrect(ix, iy, iw, ih);
  findline::setrect(ix, iy, iw, ih);
  fastmatch::setthre(8);
  fastmatch::setcomparegap(2);
  fastmatch::setmethod(0);
  fastmatch::setlinegap(1);
  fastmatch::SetWHgap(1, 1);
  fastmatch::setlinesamplerate(0.002);
  fastmatch::setfilter(0, 0, 10000);
  fastmatch::learn(g_pbackobjectimage);
  fastmatch::setmatchrect(ix, iy, iw, ih);
  fastmatch::setfindnum(1);
  fastmatch::setmatchthre(3);
  fastmatch::setfindnum(1);
  fastmatch::setmatchthre(3);
  fastmatch::match(g_pbackobjectimage);

  fastmatch::imagelearnmass(-1, 1, igrid);

  std::String qsavedir;
  if (12 == igrid)
    qsavedir = std::String("./model/12x12/");
  else if (24 == igrid)
    qsavedir = std::String("./model/24x24/");
  else if (36 == igrid)
    qsavedir = std::String("./model/36x36/");
  else if (72 == igrid)
    qsavedir = std::String("./model/72x72/");
  else if (144 == igrid)
    qsavedir = std::String("./model/144x144/");

  std::String strfilename = pfilename;

  QRegExp rxnum("(\\d+)");
  std::StringList listother = strfilename.split(rxnum);

  QRegExp rxother("(\\D+)");
  std::StringList listnum = strfilename.split(rxother);

  std::String strkey = strfilename;
  std::String strkey2;
  std::String strkey3;
  std::String strlast;
  if (listother.size() > 1) {
    std::String qstr = listother[0];
    if (qstr != "" && qstr.size() == 1)
      strkey = qstr;
    else if (qstr.size() == 2) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
    } else if (qstr.size() == 3) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
      strkey3 = qstr.mid(2, 1);
    }

    if (listnum.size() < 2) {
      strkey = listnum[0].mid(0, 1);
      strlast = listnum[0].mid(1);
    } else if (listnum[1] != "")
      strlast = listnum[1];
  }
  std::String strbase;
  std::String charstring = char2string(strkey);
  if (!strkey2.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey2);
  }
  if (!strkey3.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey3);
  }
  strbase = charstring;

  std::String qstrsavepat =
      qsavedir + strbase + std::String("_") + strlast + std::String(".pat");
  std::String qstrsaveimp =
      qsavedir + strbase + std::String("_") + strlast + std::String(".imp");
  std::String qstrbase = strbase + std::String("_") + strlast;

  fastmatch::savefastimagemodel(qstrsaveimp.toStdString().c_str());
  fastmatch::savefastimagepatmodel(qstrsavepat.toStdString().c_str());

  return;

  int ifontsize = m_fontlist_l12.size();
  for (int in = 0; in < ifontsize; in++) {

    std::String qstr = m_fontlist_l12[in];
    if (strfilename == qstr) {
      fastmatch::setcurmodels(in);
      fastmatch::setcurimagemodels(in);
      break;
    }
    if (in == ifontsize - 1) {
      fastmatch::addimagemodels_l12(qstrsaveimp.toStdString().c_str());

      m_filenamelist_l12.append(qstrbase);
      m_fontlist_l12.append(strfilename);
    }
  }

  ix = arectrecover.x();
  iy = arectrecover.y();
  iw = arectrecover.width();
  ih = arectrecover.height();

  Shape::setrect(ix, iy, iw, ih);

  levelmodel();
}

void EasyOCR::checklearn(const char *pfilename) {

  if (m_idebugrectsnum < 0)
    return;
  Rect arect = g_pbackfindobject->getgridex(m_idebugrectsnum);

  int imaxlen = g_pbackfindobject->getobjectgridw();
  int igrid = fastmatch::GetRectGridLevel(imaxlen);

  int ix = arect.x();
  int iy = arect.y();
  int iw = arect.width();
  int ih = arect.height();

  Rect arectrecover = Shape::rect();

  fastmatch::setmatchrect(ix, iy, iw, ih);
  findline::setrect(ix, iy, iw, ih);
  fastmatch::setthre(8);
  fastmatch::setcomparegap(2);
  fastmatch::setmethod(0);
  fastmatch::setlinegap(1);
  fastmatch::SetWHgap(1, 1);
  fastmatch::setlinesamplerate(0.002);
  fastmatch::setfilter(0, 0, 10000);
  fastmatch::learn(g_pbackobjectimage);
  fastmatch::setmatchrect(ix, iy, iw, ih);
  fastmatch::setfindnum(1);
  fastmatch::setmatchthre(3);
  fastmatch::setfindnum(1);
  fastmatch::setmatchthre(3);
  fastmatch::match(g_pbackobjectimage);

  fastmatch::imagelearncheck(m_imagetype, 1, igrid);

  std::String qsavedir;
  if (12 == igrid)
    qsavedir = std::String("./model/12x12/");
  else if (24 == igrid)
    qsavedir = std::String("./model/24x24/");
  else if (36 == igrid)
    qsavedir = std::String("./model/36x36/");
  else if (72 == igrid)
    qsavedir = std::String("./model/72x72/");
  else if (144 == igrid)
    qsavedir = std::String("./model/144x144/");

  std::String strfilename = pfilename;

  QRegExp rxnum("(\\d+)");
  std::StringList listother = strfilename.split(rxnum);

  QRegExp rxother("(\\D+)");
  std::StringList listnum = strfilename.split(rxother);

  std::String strkey = strfilename;
  std::String strkey2;
  std::String strkey3;
  std::String strlast;
  if (listother.size() > 1) {
    std::String qstr = listother[0];
    if (qstr != "" && qstr.size() == 1)
      strkey = qstr;
    else if (qstr.size() == 2) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
    } else if (qstr.size() == 3) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
      strkey3 = qstr.mid(2, 1);
    }

    if (listnum.size() < 2) {
      strkey = listnum[0].mid(0, 1);
      strlast = listnum[0].mid(1);
    } else if (listnum[1] != "")
      strlast = listnum[1];
  }
  std::String strbase;
  std::String charstring = char2string(strkey);
  if (!strkey2.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey2);
  }
  if (!strkey3.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey3);
  }

  strbase = charstring;

  std::String qstrsavepat =
      qsavedir + strbase + std::String("_") + strlast + std::String(".pat");
  std::String qstrsaveimp =
      qsavedir + strbase + std::String("_") + strlast + std::String(".imp");
  std::String qstrbase = strbase + std::String("_") + strlast;

  fastmatch::savefastimagemodel(qstrsaveimp.toStdString().c_str());
  fastmatch::savemodelfile(qstrsavepat.toStdString().c_str());

  return;

  int ifontsize = m_fontlist_l12.size();
  for (int in = 0; in < ifontsize; in++) {

    std::String qstr = m_fontlist_l12[in];
    if (strfilename == qstr) {
      fastmatch::setcurmodels(in);
      fastmatch::setcurimagemodels(in);
      break;
    }
    if (in == ifontsize - 1) {
      fastmatch::addimagemodels_l12(qstrsaveimp.toStdString().c_str());

      m_filenamelist_l12.append(qstrbase);
      m_fontlist_l12.append(strfilename);
    }
  }

  ix = arectrecover.x();
  iy = arectrecover.y();
  iw = arectrecover.width();
  ih = arectrecover.height();

  Shape::setrect(ix, iy, iw, ih);

  levelmodel();
}
void EasyOCR::checkmatch(const char *pfilename) {
  if (m_idebugrectsnum < 0)
    return;
  Rect arect = g_pbackfindobject->getgridex(m_idebugrectsnum);

  int imaxlen = g_pbackfindobject->getobjectgridw();
  int igrid = fastmatch::GetRectGridLevel(imaxlen);

  int ix = arect.x();
  int iy = arect.y();
  int iw = arect.width();
  int ih = arect.height();

  Rect arectrecover = Shape::rect();

  fastmatch::setmatchrect(ix, iy, iw, ih);
  findline::setrect(ix, iy, iw, ih);

  fastmatch::setthre(8);
  fastmatch::setcomparegap(2);
  fastmatch::setmethod(0);
  fastmatch::setlinegap(1);
  fastmatch::SetWHgap(1, 1);
  fastmatch::setlinesamplerate(0.002);
  fastmatch::setfilter(0, 0, 10000);

  if (1) {

    std::String qsavedir;
    if (12 == igrid)
      qsavedir = std::String("./model/12x12/");
    else if (24 == igrid)
      qsavedir = std::String("./model/24x24/");
    else if (36 == igrid)
      qsavedir = std::String("./model/36x36/");
    else if (72 == igrid)
      qsavedir = std::String("./model/72x72/");
    else if (144 == igrid)
      qsavedir = std::String("./model/144x144/");

    std::String strfilename = pfilename;

    QRegExp rxnum("(\\d+)");
    std::StringList listother = strfilename.split(rxnum);

    QRegExp rxother("(\\D+)");
    std::StringList listnum = strfilename.split(rxother);

    std::String strkey = strfilename;
    std::String strkey2;
    std::String strkey3;
    std::String strlast;
    if (listother.size() > 1) {
      std::String qstr = listother[0];
      if (qstr != "" && qstr.size() == 1)
        strkey = qstr;
      else if (qstr.size() == 2) {
        strkey = qstr.mid(0, 1);
        strkey2 = qstr.mid(1, 1);
      } else if (qstr.size() == 3) {
        strkey = qstr.mid(0, 1);
        strkey2 = qstr.mid(1, 1);
        strkey3 = qstr.mid(2, 1);
      }

      if (listnum.size() < 2) {
        strkey = listnum[0].mid(0, 1);
        strlast = listnum[0].mid(1);
      } else if (listnum[1] != "")
        strlast = listnum[1];
    }
    std::String strbase;
    std::String charstring = char2string(strkey);
    if (!strkey2.isEmpty()) {
      charstring = charstring + std::String("_") + char2string(strkey2);
    }
    if (!strkey3.isEmpty()) {
      charstring = charstring + std::String("_") + char2string(strkey3);
    }

    strbase = charstring;

    std::String qstrsavepat =
        qsavedir + strbase + std::String("_") + strlast + std::String(".pat");
    std::String qstrsaveimp =
        qsavedir + strbase + std::String("_") + strlast + std::String(".imp");
    std::String qstrbase = strbase + std::String("_") + strlast;

    fastmatch::loadmodelfile(qstrsavepat.toStdString().c_str());
    fastmatch::loadfastimagemodel(qstrsaveimp.toStdString().c_str());
  }

  fastmatch::setmatchrect(ix, iy, iw, ih);
  fastmatch::setfindnum(1);
  fastmatch::setmatchthre(3);
  fastmatch::setfindnum(1);
  fastmatch::setmatchthre(3);
  fastmatch::match(g_pbackobjectimage);

  fastmatch::imagelearncheck(m_imagetype, 1, igrid);

  return;
}
void EasyOCR::match72() {
  if (m_idebugrectsnum < 0)
    return;
  Rect arect = g_pbackfindobject->getgridex(m_idebugrectsnum);

  int imaxlen = g_pbackfindobject->getobjectgridw();
  int igrid = fastmatch::GetRectGridLevel(imaxlen);

  int ix = arect.x();
  int iy = arect.y();
  int iw = arect.width();
  int ih = arect.height();

  Rect arectrecover = Shape::rect();

  if (1) {
    fastmatch::setmatchrect(ix, iy, iw, ih);
    fastmatch::setfindnum(1);
    fastmatch::setmatchthre(3);
    fastmatch::setfindnum(1);
    fastmatch::setmatchthre(3);
    fastmatch::match(g_pbackobjectimage);
    double dvalue = fastmatch::getmaxresult();

    double dimagevalue = 0;
    fastmatch::imagelearncheck(m_imagetype, 1, igrid);
    dimagevalue = fastmatch::getimagemodelreslut();
    m_dvalue = dvalue;
    m_dmaxvalue = dimagevalue;
  }

  std::String strt =
      std::String(" %1 ").arg(m_dvalue) + std::String(" %1").arg(m_dmaxvalue);
  Shape::setname(strt.toStdString().c_str());
}
void EasyOCR::match72_matchpat() {
  if (m_idebugrectsnum < 0)
    return;
  Rect arect = g_pbackfindobject->getgridex(m_idebugrectsnum);

  int ix = arect.x();
  int iy = arect.y();
  int iw = arect.width();
  int ih = arect.height();

  Rect arectrecover = Shape::rect();

  if (1) {
    fastmatch::setmatchrect(ix, iy, iw, ih);
    fastmatch::setfindnum(1);
    fastmatch::setmatchthre(3);
    fastmatch::setfindnum(1);
    fastmatch::setmatchthre(3);
    fastmatch::match(g_pbackobjectimage);
    double dvalue = fastmatch::getmaxresult();
    m_dvalue = dvalue;
  }

  std::String strt = std::String(" %1 ").arg(m_dvalue);
  Shape::setname(strt.toStdString().c_str());
}
void EasyOCR::match72_matchimg() {
  int imaxlen = g_pbackfindobject->getobjectgridw();
  int igrid = fastmatch::GetRectGridLevel(imaxlen);
  double dimagevalue = 0;
  fastmatch::imagelearncheck(m_imagetype, 1, igrid);
  dimagevalue = fastmatch::getimagemodelreslut();
  m_dmaxvalue = dimagevalue;

  std::String strt = std::String(" %1 %2").arg(m_dvalue).arg(m_dmaxvalue);
  Shape::setname(strt.toStdString().c_str());
}
void EasyOCR::learnmass_36(const char *pfilename) {
  Rect arect = g_pbackfindobject->getgridex(m_idebugrectsnum);

  int iobjw = g_pbackfindobject->getresultw(m_idebugrectsnum);
  int iobjh = g_pbackfindobject->getresulth(m_idebugrectsnum);
  int imaxlen = iobjw > iobjh ? iobjw : iobjh;
  int igrid = 36;

  int ix = arect.x();
  int iy = arect.y();
  int iw = arect.width();
  int ih = arect.height();
  Rect arectrecover = Shape::rect();

  fastmatch::setmatchrect(ix, iy, iw, ih);
  findline::setrect(ix, iy, iw, ih);
  fastmatch::setthre(8);
  fastmatch::setcomparegap(2);
  fastmatch::setmethod(0);
  fastmatch::setlinegap(1);
  fastmatch::SetWHgap(1, 1);
  fastmatch::setlinesamplerate(0.002);
  fastmatch::setfilter(0, 0, 10000);
  fastmatch::learn(g_pbackobjectimage);
  fastmatch::setmatchrect(ix, iy, iw, ih);
  fastmatch::setfindnum(1);
  fastmatch::setmatchthre(3);
  fastmatch::setfindnum(1);
  fastmatch::setmatchthre(3);
  fastmatch::match(g_pbackobjectimage);

  fastmatch::imagelearnmass(-1, 1, igrid);

  std::String qsavedir;
  if (12 == igrid)
    qsavedir = std::String("./model/12x12/");
  else if (24 == igrid)
    qsavedir = std::String("./model/24x24/");
  else if (36 == igrid)
    qsavedir = std::String("./model/36x36/");
  else if (72 == igrid)
    qsavedir = std::String("./model/72x72/");
  else if (144 == igrid)
    qsavedir = std::String("./model/144x144/");
  else
    qsavedir = std::String("./model/12x12/");

  std::String strfilename = pfilename;

  QRegExp rxnum("(\\d+)");
  std::StringList listother = strfilename.split(rxnum);

  QRegExp rxother("(\\D+)");
  std::StringList listnum = strfilename.split(rxother);

  std::String strkey = strfilename;
  std::String strkey2;
  std::String strkey3;
  std::String strlast;
  if (listother.size() > 1) {
    std::String qstr = listother[0];
    if (qstr != "" && qstr.size() == 1)
      strkey = qstr;
    else if (qstr.size() == 2) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
    } else if (qstr.size() == 3) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
      strkey3 = qstr.mid(2, 1);
    }

    if (listnum.size() < 2) {
      strkey = listnum[0].mid(0, 1);
      strlast = listnum[0].mid(1);
    } else if (listnum[1] != "")
      strlast = listnum[1];
  }
  std::String strbase;
  std::String charstring = char2string(strkey);
  if (!strkey2.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey2);
  }
  if (!strkey3.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey3);
  }
  strbase = charstring;

  std::String qstrsavepat =
      qsavedir + strbase + std::String("_") + strlast + std::String(".pat");
  std::String qstrsaveimp =
      qsavedir + strbase + std::String("_") + strlast + std::String(".imp");
  std::String qstrbase = strbase + std::String("_") + strlast;

  fastmatch::savefastimagemodel(qstrsaveimp.toStdString().c_str());
  fastmatch::savefastimagepatmodel(qstrsavepat.toStdString().c_str());

  return;

  int ifontsize = m_fontlist_l12.size();
  for (int in = 0; in < ifontsize; in++) {

    std::String qstr = m_fontlist_l12[in];
    if (strfilename == qstr) {
      fastmatch::setcurmodels(in);
      fastmatch::setcurimagemodels(in);
      break;
    }
    if (in == ifontsize - 1) {
      fastmatch::addimagemodels_l12(qstrsaveimp.toStdString().c_str());

      m_filenamelist_l12.append(qstrbase);
      m_fontlist_l12.append(strfilename);
    }
  }

  ix = arectrecover.x();
  iy = arectrecover.y();
  iw = arectrecover.width();
  ih = arectrecover.height();

  Shape::setrect(ix, iy, iw, ih);

  levelmodel();
}

void EasyOCR::setlevelstring() {
  int istrsize = m_fontlist_l12.size();
  int istrsize1 = m_fontlist_l36.size();

  int ijsize = getlevel6_12map().size();
  int iksize = getlevel12_36map().size();
  int ilsize = getlevel36_72map().size();

  int iaddmax = ijsize - istrsize;
  int iaddmax1 = iksize - istrsize1;
  for (int il = 0; il < ilsize; il++) {
    int ilnum = getlevel36_72map()[il];
    if (ilnum >= istrsize1) {
      for (int iz = 0; iz < iaddmax1; iz++) {
        if (ilnum == istrsize1 + iz) {
          m_fontlist_l36.push_back(m_fontlist_l72[il]);
        }
      }
    }
  }
  for (int ik = 0; ik < iksize; ik++) {
    int iknum = getlevel12_36map()[ik];
    if (iknum >= istrsize) {
      for (int iz = 0; iz < iaddmax; iz++) {
        if (iknum == istrsize + iz) {
          m_fontlist_l12.push_back(m_fontlist_l36[ik]);
        }
      }
    }
  }
}
void EasyOCR::levelmodel() {
  list_duplicatesmodel_l12();
  list_duplicatesmodel_l36();
  list_duplicatesmodel_l72();

  levelmodels_l72tol36();
  levelmodels_l36tol12();
  levelmodels_l12tol6();
  levelmodels_l6tol3();
}
void EasyOCR::setgrid(int iw, int igrid) { fastmatch::setgrid(iw, igrid); }
void EasyOCR::savelevelmodel() { savelevel0_l1(); }
void EasyOCR::setmatchvalid(double dthre) { m_dmatchthre = dthre; }
void EasyOCR::setusingobject(int iusing) { m_icompareobject = iusing; }
void EasyOCR::fontocr() {
  m_ilevle = 2;
  m_resultstrlist.clear();
  m_resultstring.clear();
  double dvalue = 0;
  double dimagevalue = 0;
  double dmaxvalue = 0;
  int imodelobjectw = 0;
  int imodelobjectb = 0;
  int imatchobjectw = 0;
  int imatchobjectb = 0;

  int iresultfont = 0;
  int iareasnum = g_pbackfindobject->getresultobjsnum();
  if (iareasnum <= 0)
    return;

  for (int ia = 0; ia < iareasnum; ia++) {
    if (-1 == m_idebugrectsnum || ia == m_idebugrectsnum) {
      int isize = m_fontlist_l12.size();
      Rect arect = g_pbackfindobject->getgrid(ia);
      int ix = arect.x();
      int iy = arect.y();
      int iw = arect.width();
      int ih = arect.height();
      fastmatch::setmatchrect(ix, iy, iw, ih);
      dmaxvalue = 0;
      iresultfont = 0;
      for (int i = 0; i < isize; i++) {
        if (-1 == m_idebugfontnum || i == m_idebugfontnum) {
          fastmatch::modelstocurrent_l12(i);
          fastmatch::imagemodelstocurrent_l12(i);
          imodelobjectw = fastmatch::getmodeleasyobjectw_l12(i);
          imodelobjectb = fastmatch::getmodeleasyobjectb_l12(i);

          fastmatch::Match(*g_pbackobjectimage);

          dvalue = fastmatch::getmaxresult();

          dimagevalue = 0;
          if (dvalue >= m_dmatchthre) {
            fastmatch::imagematch(-1, 1);
            dimagevalue = fastmatch::getimagemodelreslut();

            imatchobjectb = fastmatch::geteasyobjectb();
            imatchobjectw = fastmatch::geteasyobjectw();
            int iobj = 1;
            if (m_icompareobject == 0) {
              iobj = 1;
            } else if (m_icompareobject == 1) {
              iobj = 0;
              if (imatchobjectb == imodelobjectb &&
                  imatchobjectw == imodelobjectw) {
                iobj = 1;
              }
            } else if (m_icompareobject == 2) {
              iobj = 0;
              if (imatchobjectb == imodelobjectb) {
                iobj = 1;
              }
            } else if (m_icompareobject == 3) {
              iobj = 0;

              if (imatchobjectw == imodelobjectw) {
                iobj = 1;
              }
            }

            if (dimagevalue > dmaxvalue && iobj > 0) {
              dmaxvalue = dimagevalue;
              iresultfont = i;
            }
            if (-1 == m_idebugfontnum && 100 == dimagevalue)
              break;
          }
        }
      }

      if (-1 == m_idebugrectsnum && -1 == m_idebugfontnum) {
        QRegExp rxnum("(\\d+)");
        std::StringList listother = m_fontlist_l12[iresultfont].split(rxnum);

        QRegExp rxother("(\\D+)");
        std::StringList listnum = m_fontlist_l12[iresultfont].split(rxother);

        std::String strkey = m_fontlist_l12[iresultfont];

        if (listother.size() > 0) {
          if (listother[0] != "")
            strkey = listother[0];
          if (listnum.size() < 2) {
            strkey = listnum[0].mid(0, 1);
          }
        }
        m_resultstrlist.push_back(strkey);
        m_resultstring.append(strkey);

      } else {
        std::String strkey = m_fontlist_l12[iresultfont];
        m_resultstrlist.push_back(strkey);
        m_resultstring.append(strkey);
      }
    }
  }
  if (-1 != m_idebugrectsnum && -1 != m_idebugfontnum) {
    std::String astr = std::String("  %1   %2 b(%3 %5)w(%4 %6)")
                           .arg(dvalue)
                           .arg(dimagevalue)
                           .arg(imatchobjectb)
                           .arg(imatchobjectw)
                           .arg(imodelobjectb)
                           .arg(imodelobjectw);
    Shape::setname(astr.toStdString().c_str());
  } else if (-1 != m_idebugrectsnum && -1 == m_idebugfontnum) {
    fastmatch::modelstocurrent_l12(iresultfont);
    fastmatch::imagemodelstocurrent_l12(iresultfont);

    fastmatch::Match(*g_pbackobjectimage);

    fastmatch::imagematch(-1, 1);
    imodelobjectw = fastmatch::getmodeleasyobjectw_l12(iresultfont);
    imodelobjectb = fastmatch::getmodeleasyobjectb_l12(iresultfont);

    imatchobjectb = fastmatch::geteasyobjectb();
    imatchobjectw = fastmatch::geteasyobjectw();
    std::String strt = std::String(" %1 ").arg(iresultfont) + m_resultstring +
                       std::String(" %1").arg(dmaxvalue);
    strt = strt + std::String(" b(%3 %5)w(%4 %6)")
                      .arg(imatchobjectb)
                      .arg(imatchobjectw)
                      .arg(imodelobjectb)
                      .arg(imodelobjectw);
    Shape::setname(strt.toStdString().c_str());
  } else
    Shape::setname(m_resultstring.toStdString().c_str());
}

void EasyOCR::SelectModel(int ilevle, int inum) {
  m_ilevle = ilevle;
  switch (ilevle) {
  case 0:
    fastmatch::modelstocurrent_l3(inum);
    fastmatch::imagemodelstocurrent_l3(inum);
    m_imodelobjectw = fastmatch::getmodeleasyobjectw_l3(inum);
    m_imodelobjectb = fastmatch::getmodeleasyobjectb_l3(inum);

    break;
  case 1:
    fastmatch::modelstocurrent_l6(inum);
    fastmatch::imagemodelstocurrent_l6(inum);
    m_imodelobjectw = fastmatch::getmodeleasyobjectw_l6(inum);
    m_imodelobjectb = fastmatch::getmodeleasyobjectb_l6(inum);

    break;
  case 2:
    fastmatch::modelstocurrent_l12(inum);
    fastmatch::imagemodelstocurrent_l12(inum);
    m_imodelobjectw = fastmatch::getmodeleasyobjectw_l12(inum);
    m_imodelobjectb = fastmatch::getmodeleasyobjectb_l12(inum);
    break;
  case 3:
    fastmatch::modelstocurrent_l36(inum);
    fastmatch::imagemodelstocurrent_l36(inum);
    m_imodelobjectw = fastmatch::getmodeleasyobjectw_l36(inum);
    m_imodelobjectb = fastmatch::getmodeleasyobjectb_l36(inum);
    break;
  case 4:
    fastmatch::modelstocurrent_l72(inum);
    fastmatch::imagemodelstocurrent_l72(inum);
    m_imodelobjectw = fastmatch::getmodeleasyobjectw_l72(inum);
    m_imodelobjectb = fastmatch::getmodeleasyobjectb_l72(inum);
    break;
  }
}

void EasyOCR::selectmodel72(const char *pfilename) {
  SelectNameModel(4, pfilename);
}

void EasyOCR::SelectNameModel(int ilevel, const char *pfilename) {
  m_ilevle = ilevel;

  int iselnum = 0;
  std::String strname(pfilename);

  switch (ilevel) {

  case 2: {
    int isize = m_filenamelist_l12.size();
    for (int i = 0; i < isize; i++) {
      if (strname == m_filenamelist_l12[i]) {
        iselnum = i;
        break;
      }
    }
  } break;

  case 3: {
    int isize = m_filenamelist_l36.size();
    for (int i = 0; i < isize; i++) {
      if (strname == m_filenamelist_l36[i]) {
        iselnum = i;
        break;
      }
    }
  } break;

  case 4: {
    int isize = m_fontlist_l72.size();
    for (int i = 0; i < isize; i++) {
      if (strname == m_fontlist_l72[i]) {
        iselnum = i;
        break;
      }
    }
  } break;
  }

  SelectModel(ilevel, iselnum);
}
void EasyOCR::modelmethod(int itype) { fastmatch::modelmethod(itype); }

void EasyOCR::fontocr_level(int ilevel) {
  int iareasnum = g_pbackfindobject->getresultobjsnum();

  m_resultstrlist.clear();
  m_resultstring.clear();
  if (iareasnum <= 0)
    return;
  m_ilevle = ilevel;
  if (0 == ilevel) {
    m_l3resultlist.clear();
  } else if (1 == ilevel) {
    m_l6resultlist.clear();
  } else if (2 == ilevel) {
    m_l12resultlist.clear();
  }
  double dvalue = 0;
  double dimagevalue = 0;
  double dmaxvalue = 0;
  int imatchobjectw = 0;
  int imatchobjectb = 0;

  int iresultfont = 0;

  for (int ia = 0; ia < iareasnum; ia++) {
    if (-1 == m_idebugrectsnum || ia == m_idebugrectsnum) {
      int isize = imagefastmodelsize(ilevel);
      Rect arect = g_pbackfindobject->getgrid(ia);
      int ix = arect.x();
      int iy = arect.y();
      int iw = arect.width();
      int ih = arect.height();
      g_pbackobjectimage->SetROI(ix, iy, iw, ih);

      switch (ilevel) {
      case 0:
        fastmatch::setmatchrect(ix, iy, iw * 0.25, ih * 0.25);

        g_pbackobjectimage->ROIColorTable();
        g_pbackobjectimage->ROIColorTableBlur(0, -1);
        g_pbackobjectimage->ROIColorTableEasyThre(1);
        g_pbackobjectimage->ROIImageZoom(g_pbackimage, 0.25, 0.25);
        break;
      case 1:
        fastmatch::setmatchrect(ix, iy, iw * 0.5, ih * 0.5);

        g_pbackobjectimage->ROIColorTable();
        g_pbackobjectimage->ROIColorTableBlur(0, -1);
        g_pbackobjectimage->ROIColorTableEasyThre(1);
        g_pbackobjectimage->ROIImageZoom(g_pbackimage, 0.5, 0.5);
        break;
      default:
      case 2:
        fastmatch::setmatchrect(ix, iy, iw, ih);

        g_pbackobjectimage->ROIColorTable();
        g_pbackobjectimage->ROIColorTableBlur(0, -1);
        g_pbackobjectimage->ROIColorTableEasyThre(1);
        g_pbackobjectimage->ROIImageZoom(g_pbackimage, 1, 1);
        break;
      }

      dmaxvalue = 0;
      int imaxnum = 0;
      iresultfont = 0;
      for (int i = 0; i < isize; i++) {
        if (-1 == m_idebugfontnum || i == m_idebugfontnum) {
          SelectModel(ilevel, i);
          fastmatch::Match(*g_pbackimage);
          dvalue = fastmatch::getmaxresult();
          dimagevalue = 0;
          if (dvalue >= dmaxvalue) {
            imaxnum = i;
            dmaxvalue = dvalue;
          }
          if ((0 == ilevel && -1 != m_idebugfontnum) || 1 == ilevel ||
              2 == ilevel) {
            if (dvalue >= m_dmatchthre) {
              switch (ilevel) {
              case 0:
                fastmatch::imagematchex(3);
                break;
              case 1:
                fastmatch::imagematchex(6);
                break;
              case 2:
                fastmatch::imagematchex(12);
                break;
              }
              dimagevalue = fastmatch::getimagemodelreslut();

              imatchobjectb = fastmatch::geteasyobjectb();
              imatchobjectw = fastmatch::geteasyobjectw();
              int iobj = 1;
              if (m_icompareobject == 0) {
                iobj = 1;
              } else if (m_icompareobject == 1) {
                iobj = 0;
                if (imatchobjectb == m_imodelobjectb &&
                    imatchobjectw == m_imodelobjectw) {
                  iobj = 1;
                }
              } else if (m_icompareobject == 2) {
                iobj = 0;
                if (imatchobjectb == m_imodelobjectb) {
                  iobj = 1;
                }
              } else if (m_icompareobject == 3) {
                iobj = 0;

                if (imatchobjectw == m_imodelobjectw) {
                  iobj = 1;
                }
              }

              if (dimagevalue > dmaxvalue && iobj > 0) {
                dmaxvalue = dimagevalue;
                iresultfont = i;
              }
              if (-1 == m_idebugfontnum && 100 == dimagevalue)
                break;
            }
          }
        }
      }

      if (-1 == m_idebugfontnum && 0 == ilevel) {
        SelectModel(ilevel, imaxnum);
        fastmatch::Match(*g_pbackimage);
        dvalue = fastmatch::getmaxresult();
        switch (ilevel) {
        case 0:
          fastmatch::imagematchex(3);
          break;
        case 1:
          fastmatch::imagematchex(6);
          break;
        case 2:
          fastmatch::imagematchex(12);
          break;
        case 3:
          fastmatch::imagematchex(36);
          break;
        }
        dimagevalue = fastmatch::getimagemodelreslut();

        imatchobjectb = fastmatch::geteasyobjectb();
        imatchobjectw = fastmatch::geteasyobjectw();
        int iobj = 1;
        if (m_icompareobject == 0) {
          iobj = 1;
        } else if (m_icompareobject == 1) {
          iobj = 0;
          if (imatchobjectb == m_imodelobjectb &&
              imatchobjectw == m_imodelobjectw) {
            iobj = 1;
          }
        } else if (m_icompareobject == 2) {
          iobj = 0;
          if (imatchobjectb == m_imodelobjectb) {
            iobj = 1;
          }
        } else if (m_icompareobject == 3) {
          iobj = 0;

          if (imatchobjectw == m_imodelobjectw) {
            iobj = 1;
          }
        }

        if (dimagevalue > dmaxvalue && iobj > 0) {
          dmaxvalue = dimagevalue;
          iresultfont = imaxnum;
        }
        m_l3resultlist.push_back(imaxnum);
      } else if (1 == ilevel) {
        m_l6resultlist.push_back(iresultfont);
      } else if (2 == ilevel) {
        if (-1 == m_idebugrectsnum && -1 == m_idebugfontnum) {
          QRegExp rxnum("(\\d+)");
          std::StringList listother = m_fontlist_l12[iresultfont].split(rxnum);

          QRegExp rxother("(\\D+)");
          std::StringList listnum = m_fontlist_l12[iresultfont].split(rxother);

          std::String strkey = m_fontlist_l12[iresultfont];
          if (listother.size() > 0) {
            if (listother[0] != "")
              strkey = listother[0];
            if (listnum.size() < 2) {
              strkey = listnum[0].mid(0, 1);
            }
          }
          m_resultstrlist.push_back(strkey);
          m_resultstring.append(strkey);

          m_l12resultlist.push_back(iresultfont);
        } else {
          std::String strkey = m_fontlist_l12[iresultfont];
          m_resultstrlist.push_back(strkey);
          m_resultstring.append(strkey);
          m_l12resultlist.push_back(iresultfont);
        }
      }
    }
  }
  if (-1 != m_idebugrectsnum && -1 != m_idebugfontnum) {
    std::String astr = std::String("  %1   %2 b(%3 %5)w(%4 %6)")
                           .arg(dvalue)
                           .arg(dimagevalue)
                           .arg(imatchobjectb)
                           .arg(imatchobjectw)
                           .arg(m_imodelobjectb)
                           .arg(m_imodelobjectw);
    Shape::setname(astr.toStdString().c_str());
  } else if (-1 != m_idebugrectsnum && -1 == m_idebugfontnum) {
    SelectModel(ilevel, iresultfont);

    fastmatch::Match(*g_pbackobjectimage);

    switch (ilevel) {
    case 0:
      fastmatch::imagematchex(3);
      break;
    case 1:
      fastmatch::imagematchex(6);
      break;
    case 2:
      fastmatch::imagematchex(12);
      break;
    case 3:
      fastmatch::imagematchex(36);
      break;
    }

    imatchobjectb = fastmatch::geteasyobjectb();
    imatchobjectw = fastmatch::geteasyobjectw();
    std::String strt = std::String(" %1 ").arg(iresultfont) + m_resultstring +
                       std::String(" %1").arg(dmaxvalue);
    strt = strt + std::String(" b(%3 %5)w(%4 %6)")
                      .arg(imatchobjectb)
                      .arg(imatchobjectw)
                      .arg(m_imodelobjectb)
                      .arg(m_imodelobjectw);
    Shape::setname(strt.toStdString().c_str());
  } else {
    if (0 == ilevel) {
      std::String qshowstr;
      int isize = m_l3resultlist.size();
      for (int i = 0; i < isize; i++) {
        std::String astrnum = std::String("%1_").arg(m_l3resultlist[i]);
        qshowstr = qshowstr + astrnum;
      }

      Shape::setname(qshowstr.toStdString().c_str());
    } else if (1 == ilevel) {
      std::String qshowstr;
      int isize = m_l6resultlist.size();
      for (int i = 0; i < isize; i++) {
        std::String astrnum = std::String("%1_").arg(m_l6resultlist[i]);
        qshowstr = qshowstr + astrnum;
      }

      Shape::setname(qshowstr.toStdString().c_str());
    } else if (2 == ilevel)
      Shape::setname(m_resultstring.toStdString().c_str());
  }
}
int EasyOCR::imagefastmapsize(int ilevel, int inum) {
  int igetsize = 0;
  if (0 == ilevel) {
    return imagefastmodelsize(ilevel);
  } else if (1 == ilevel) {
    int il0num = m_l3resultlist[inum];
    int isize = getlevel3_6map().size();
    for (int i = 0; i < isize; i++) {
      if (getlevel3_6map()[i] == il0num) {
        igetsize = igetsize + 1;
      }
    }
    return igetsize;
  } else if (2 == ilevel) {
    int il1num = m_l6resultlist[inum];
    int isize = getlevel6_12map().size();
    for (int i = 0; i < isize; i++) {
      if (getlevel6_12map()[i] == il1num) {
        igetsize = igetsize + 1;
      }
    }
    return igetsize;
  } else if (3 == ilevel) {
    int il1num = m_l12resultlist[inum];
    int isize = getlevel12_36map().size();
    for (int i = 0; i < isize; i++) {
      if (getlevel12_36map()[i] == il1num) {
        igetsize = igetsize + 1;
      }
    }
    return igetsize;
  }
  return 0;
}
int EasyOCR::SelectMapModel(int ilevel, int inum, int i0) {
  m_ilevle = ilevel;
  int igetsize = 0;
  if (0 == ilevel) {
    SelectModel(ilevel, i0);
    return i0;
  } else if (1 == ilevel) {
    int il0num = m_l3resultlist[inum];
    int isize = getlevel3_6map().size();
    for (int i = 0; i < isize; i++) {
      if (getlevel3_6map()[i] == il0num) {
        if (igetsize == i0) {
          fastmatch::modelstocurrent_l6(i);
          fastmatch::imagemodelstocurrent_l6(i);
          m_imodelobjectw = fastmatch::getmodeleasyobjectw_l6(i);
          m_imodelobjectb = fastmatch::getmodeleasyobjectb_l6(i);
          return i;
        }
        igetsize = igetsize + 1;
      }
    }
  } else if (2 == ilevel) {
    int il1num = m_l6resultlist[inum];
    int isize = getlevel6_12map().size();
    for (int i = 0; i < isize; i++) {
      if (getlevel3_6map()[i] == il1num) {
        if (igetsize == i0) {
          fastmatch::modelstocurrent_l12(i);
          fastmatch::imagemodelstocurrent_l12(i);
          m_imodelobjectw = fastmatch::getmodeleasyobjectw_l12(i);
          m_imodelobjectb = fastmatch::getmodeleasyobjectb_l12(i);
          return i;
        }
        igetsize = igetsize + 1;
      }
    }
  } else if (3 == ilevel) {
    int il1num = m_l12resultlist[inum];
    int isize = getlevel12_36map().size();
    for (int i = 0; i < isize; i++) {
      if (getlevel12_36map()[i] == il1num) {
        if (igetsize == i0) {
          fastmatch::modelstocurrent_l36(i);
          fastmatch::imagemodelstocurrent_l36(i);
          m_imodelobjectw = fastmatch::getmodeleasyobjectw_l36(i);
          m_imodelobjectb = fastmatch::getmodeleasyobjectb_l36(i);
          return i;
        }
        igetsize = igetsize + 1;
      }
    }
  }
  return 0;
}
int EasyOCR::resultnodesize(int ilevel, int iareanum) {
  if (0 == ilevel) {
    return imagefastmodelsize(ilevel);
  } else {
    levelnode anode = m_reslutnodelist[iareanum];
    int iselectnum = 0;
    int isize = getlevel3_6map().size();
    int jsize = getlevel6_12map().size();
    int ksize = getlevel12_36map().size();
    if (0 == anode.s_ilevel) {
      if (anode.s_inode < isize && anode.s_inode >= 0) {
        if (0 == ilevel) {
          return 1;
        } else if (1 == ilevel) {
          for (int i = 0; i < isize; i++) {
            if (getlevel3_6map()[i] == anode.s_inode) {
              iselectnum = iselectnum + 1;
            }
          }
          return iselectnum;
        } else if (2 == ilevel) {
          for (int i = 0; i < isize; i++) {
            if (getlevel3_6map()[i] == anode.s_inode) {
              for (int j = 0; j < jsize; j++) {
                if (getlevel6_12map()[j] == i) {
                  iselectnum = iselectnum + 1;
                }
              }
            }
          }
          return iselectnum;
        }
      } else {
        return imagefastmodelsize(ilevel);
      }
    } else if (1 == anode.s_ilevel) {
      if (anode.s_inode < isize && anode.s_inode >= 0) {
        if (0 == ilevel) {
          return 1;
        } else if (1 == ilevel) {
          return 1;
        } else if (2 == ilevel) {
          for (int j = 0; j < jsize; j++) {
            if (getlevel6_12map()[j] == anode.s_inode) {
              iselectnum = iselectnum + 1;
            }
          }
          return iselectnum;
        }
      } else {
        return imagefastmodelsize(ilevel);
      }

    } else if (2 == anode.s_ilevel) {
      if (anode.s_inode < jsize && anode.s_inode >= 0) {
        if (ilevel < 3)
          return 1;
        else if (3 == ilevel) {
          for (int k = 0; k < ksize; k++) {
            if (getlevel12_36map()[k] == anode.s_inode) {
              iselectnum = iselectnum + 1;
            }
          }
          return iselectnum;
        }
      } else {
        return imagefastmodelsize(ilevel);
      }
    }
  }
}
void EasyOCR::selectresultnode(int ilevel, int iareanum, int inum0) {
  if (m_reslutnodelist.size() <= iareanum)
    return;
  levelnode anode = m_reslutnodelist[iareanum];
  int iselectnum = 0;
  int isize = getlevel3_6map().size();
  int jsize = getlevel6_12map().size();
  int ksize = getlevel12_36map().size();
  if (0 == anode.s_ilevel) {
    if (anode.s_inode < isize && anode.s_inode >= 0) {
      if (0 == ilevel) {
        fastmatch::modelstocurrent_l3(inum0);
        fastmatch::imagemodelstocurrent_l3(inum0);
        m_imodelobjectw = fastmatch::getmodeleasyobjectw_l3(inum0);
        m_imodelobjectb = fastmatch::getmodeleasyobjectb_l3(inum0);
        m_selectnode.s_ilevel = 0;
        m_selectnode.s_inode = inum0;
        return;
      } else if (1 == ilevel) {
        for (int i = 0; i < isize; i++) {
          if (getlevel3_6map()[i] == anode.s_inode) {
            if (iselectnum == inum0) {
              fastmatch::modelstocurrent_l6(i);
              fastmatch::imagemodelstocurrent_l6(i);
              m_imodelobjectw = fastmatch::getmodeleasyobjectw_l6(i);
              m_imodelobjectb = fastmatch::getmodeleasyobjectb_l6(i);
              m_selectnode.s_ilevel = 1;
              m_selectnode.s_inode = i;
              return;
            }
            iselectnum = iselectnum + 1;
          }
        }
      } else if (2 == ilevel) {
        for (int i = 0; i < isize; i++) {
          if (getlevel3_6map()[i] == anode.s_inode) {
            for (int j = 0; j < jsize; j++) {
              if (getlevel6_12map()[j] == i) {
                if (iselectnum == inum0) {
                  fastmatch::modelstocurrent_l12(j);
                  fastmatch::imagemodelstocurrent_l12(j);
                  m_imodelobjectw = fastmatch::getmodeleasyobjectw_l12(j);
                  m_imodelobjectb = fastmatch::getmodeleasyobjectb_l12(j);
                  m_selectnode.s_ilevel = 2;
                  m_selectnode.s_inode = j;
                  return;
                }
                iselectnum = iselectnum + 1;
              }
            }
          }
        }
      }
    } else {
      if (0 == ilevel) {
        fastmatch::modelstocurrent_l3(inum0);
        fastmatch::imagemodelstocurrent_l3(inum0);
        m_imodelobjectw = fastmatch::getmodeleasyobjectw_l3(inum0);
        m_imodelobjectb = fastmatch::getmodeleasyobjectb_l3(inum0);
        m_selectnode.s_ilevel = 0;
        m_selectnode.s_inode = inum0;
        return;

      } else if (1 == ilevel) {
        fastmatch::modelstocurrent_l6(inum0);
        fastmatch::imagemodelstocurrent_l6(inum0);
        m_imodelobjectw = fastmatch::getmodeleasyobjectw_l6(inum0);
        m_imodelobjectb = fastmatch::getmodeleasyobjectb_l6(inum0);
        m_selectnode.s_ilevel = 1;
        m_selectnode.s_inode = inum0;
        return;

      } else if (2 == ilevel) {
        fastmatch::modelstocurrent_l12(inum0);
        fastmatch::imagemodelstocurrent_l12(inum0);
        m_imodelobjectw = fastmatch::getmodeleasyobjectw_l12(inum0);
        m_imodelobjectb = fastmatch::getmodeleasyobjectb_l12(inum0);
        m_selectnode.s_ilevel = 2;
        m_selectnode.s_inode = inum0;
        return;
      }
    }
  } else if (1 == anode.s_ilevel) {
    if (anode.s_inode < isize && anode.s_inode >= 0) {
      if (0 == ilevel) {
        fastmatch::modelstocurrent_l6(anode.s_inode);
        fastmatch::imagemodelstocurrent_l6(anode.s_inode);
        m_imodelobjectw = fastmatch::getmodeleasyobjectw_l6(anode.s_inode);
        m_imodelobjectb = fastmatch::getmodeleasyobjectb_l6(anode.s_inode);
        m_selectnode.s_ilevel = 1;
        m_selectnode.s_inode = anode.s_inode;
        return;
      } else if (1 == ilevel) {
        fastmatch::modelstocurrent_l6(anode.s_inode);
        fastmatch::imagemodelstocurrent_l6(anode.s_inode);
        m_imodelobjectw = fastmatch::getmodeleasyobjectw_l6(anode.s_inode);
        m_imodelobjectb = fastmatch::getmodeleasyobjectb_l6(anode.s_inode);
        m_selectnode.s_ilevel = 1;
        m_selectnode.s_inode = anode.s_inode;
        return;
      } else if (2 == ilevel) {
        for (int j = 0; j < jsize; j++) {
          if (getlevel6_12map()[j] == anode.s_inode) {
            if (iselectnum == inum0) {
              fastmatch::modelstocurrent_l12(j);
              fastmatch::imagemodelstocurrent_l12(j);
              m_imodelobjectw = fastmatch::getmodeleasyobjectw_l12(j);
              m_imodelobjectb = fastmatch::getmodeleasyobjectb_l12(j);
              m_selectnode.s_ilevel = 2;
              m_selectnode.s_inode = j;
              return;
            }
            iselectnum = iselectnum + 1;
          }
        }
      }
    } else {
      if (0 == ilevel) {
        fastmatch::modelstocurrent_l3(inum0);
        fastmatch::imagemodelstocurrent_l3(inum0);
        m_imodelobjectw = fastmatch::getmodeleasyobjectw_l3(inum0);
        m_imodelobjectb = fastmatch::getmodeleasyobjectb_l3(inum0);
        m_selectnode.s_ilevel = 0;
        m_selectnode.s_inode = inum0;
        return;
      } else if (1 == ilevel) {
        fastmatch::modelstocurrent_l6(inum0);
        fastmatch::imagemodelstocurrent_l6(inum0);
        m_imodelobjectw = fastmatch::getmodeleasyobjectw_l6(inum0);
        m_imodelobjectb = fastmatch::getmodeleasyobjectb_l6(inum0);
        m_selectnode.s_ilevel = 1;
        m_selectnode.s_inode = inum0;
        return;
      } else if (2 == ilevel) {
        fastmatch::modelstocurrent_l12(inum0);
        fastmatch::imagemodelstocurrent_l12(inum0);
        m_imodelobjectw = fastmatch::getmodeleasyobjectw_l12(inum0);
        m_imodelobjectb = fastmatch::getmodeleasyobjectb_l12(inum0);
        m_selectnode.s_ilevel = 2;
        m_selectnode.s_inode = inum0;
        return;
      }
    }
  } else if (2 == anode.s_ilevel) {
    if (anode.s_inode < jsize && anode.s_inode >= 0) {
      if (ilevel < 3) {
        fastmatch::modelstocurrent_l12(anode.s_inode);
        fastmatch::imagemodelstocurrent_l12(anode.s_inode);
        m_imodelobjectw = fastmatch::getmodeleasyobjectw_l12(anode.s_inode);
        m_imodelobjectb = fastmatch::getmodeleasyobjectb_l12(anode.s_inode);
        m_selectnode.s_ilevel = 2;
        m_selectnode.s_inode = anode.s_inode;
      } else if (3 == ilevel) {
        for (int k = 0; k < ksize; k++) {
          if (getlevel12_36map()[k] == anode.s_inode) {
            if (iselectnum == inum0) {
              fastmatch::modelstocurrent_l36(k);
              fastmatch::imagemodelstocurrent_l36(k);
              m_imodelobjectw = fastmatch::getmodeleasyobjectw_l36(k);
              m_imodelobjectb = fastmatch::getmodeleasyobjectb_l36(k);
              m_selectnode.s_ilevel = 3;
              m_selectnode.s_inode = k;
              return;
            }
            iselectnum = iselectnum + 1;
          }
        }
      }

      return;
    } else {

      if (0 == ilevel) {
        fastmatch::modelstocurrent_l3(inum0);
        fastmatch::imagemodelstocurrent_l3(inum0);
        m_imodelobjectw = fastmatch::getmodeleasyobjectw_l3(inum0);
        m_imodelobjectb = fastmatch::getmodeleasyobjectb_l3(inum0);
        m_selectnode.s_ilevel = 0;
        m_selectnode.s_inode = inum0;
        return;

      } else if (1 == ilevel) {
        fastmatch::modelstocurrent_l6(inum0);
        fastmatch::imagemodelstocurrent_l6(inum0);
        m_imodelobjectw = fastmatch::getmodeleasyobjectw_l6(inum0);
        m_imodelobjectb = fastmatch::getmodeleasyobjectb_l6(inum0);
        m_selectnode.s_ilevel = 1;
        m_selectnode.s_inode = inum0;
        return;

      } else if (2 == ilevel) {
        fastmatch::modelstocurrent_l12(inum0);
        fastmatch::imagemodelstocurrent_l12(inum0);
        m_imodelobjectw = fastmatch::getmodeleasyobjectw_l12(inum0);
        m_imodelobjectb = fastmatch::getmodeleasyobjectb_l12(inum0);
        m_selectnode.s_ilevel = 2;
        m_selectnode.s_inode = inum0;
        return;
      }
    }
  }
}
void EasyOCR::resultnodereset(int iareasnum) {
  m_reslutnodelist.clear();
  for (int i = 0; i < iareasnum; i++) {
    levelnode anode;
    anode.s_ilevel = 0;
    anode.s_inode = -1;
    m_reslutnodelist.push_back(anode);
  }
}
void EasyOCR::setresultnode(int inum, levelnode inode) {
  m_reslutnodelist[inum] = inode;
}

void EasyOCR::fontocr_levelex(int ilevel) {
  int iareasnum = g_pbackfindobject->getresultobjsnum();

  m_resultstrlist.clear();
  m_resultstring.clear();
  if (iareasnum <= 0)
    return;
  m_ilevle = ilevel;
  if (0 == ilevel) {
    resultnodereset(iareasnum);
    m_l3resultlist.clear();
  } else if (1 == ilevel) {
    if (m_l3resultlist.size() <= 0)
      return;
    if (m_l3resultlist.size() != iareasnum)
      return;
    m_l6resultlist.clear();
  } else if (2 == ilevel) {
    if (m_l6resultlist.size() <= 0)
      return;
    if (m_l6resultlist.size() != iareasnum)
      return;
  } else if (3 == ilevel) {
    if (m_l12resultlist.size() <= 0)
      return;
    if (m_l12resultlist.size() != iareasnum)
      return;
  }
  double dvalue = 0;
  double dimagevalue = 0;
  double dmaxvalue = 0;
  int iselmaxnum = 0;
  int imatchobjectw = 0;
  int imatchobjectb = 0;

  int iresultfont = 0;

  for (int ia = 0; ia < iareasnum; ia++) {
    if (-1 == m_idebugrectsnum || ia == m_idebugrectsnum) {
      int isize = imagefastmapsize(ilevel, ia);

      Rect arect = g_pbackfindobject->getgrid(ia);
      int ix = arect.x();
      int iy = arect.y();
      int iw = arect.width();
      int ih = arect.height();
      g_pbackobjectimage->SetROI(ix, iy, iw, ih);

      switch (ilevel) {
      case 0:
        fastmatch::setmatchrect(ix, iy, iw * 0.25, ih * 0.25);

        g_pbackobjectimage->ROIColorTable();
        g_pbackobjectimage->ROIColorTableBlur(0, -1);
        g_pbackobjectimage->ROIColorTableEasyThre(1);

        m_pimagegrid->ROIImagetoModel(*g_pbackobjectimage);

        if (0) {
          m_pimagegrid->SetUnit(12, 12);
          m_pimagegrid->UnitGrid();
        }
        m_pimagegrid->ZeroModel();
        m_pimagegrid->ReGrid(12, 12);
        m_pimagegrid->GridZoom(3, 3);
        m_pimagegrid->SetUnit(3, 3);

        break;
      case 1:
        fastmatch::setmatchrect(ix, iy, iw * 0.5, ih * 0.5);

        g_pbackobjectimage->ROIColorTable();
        g_pbackobjectimage->ROIColorTableBlur(0, -1);
        g_pbackobjectimage->ROIColorTableEasyThre(1);

        m_pimagegrid->ROIImagetoModel(*g_pbackobjectimage);

        if (0) {
          m_pimagegrid->SetUnit(12, 12);
          m_pimagegrid->UnitGrid();
        }
        m_pimagegrid->ZeroModel();
        m_pimagegrid->ReGrid(12, 12);

        m_pimagegrid->GridZoom(6, 6);
        m_pimagegrid->SetUnit(6, 6);
        break;
      default:
      case 2:
        fastmatch::setmatchrect(ix, iy, iw, ih);

        g_pbackobjectimage->ROIColorTable();
        g_pbackobjectimage->ROIColorTableBlur(0, -1);
        g_pbackobjectimage->ROIColorTableEasyThre(1);

        break;
      }

      dmaxvalue = 0;
      iselmaxnum = 0;
      int imaxnum = 0;
      iresultfont = 0;
      for (int i = 0; i < isize; i++) {
        int iselnum = SelectMapModel(ilevel, ia, i);
        if (-1 == m_idebugfontnum || iselnum == m_idebugfontnum) {
          MatchGrid(m_pimagegrid);
          dimagevalue = fastmatch::getimagemodelreslut();

          imatchobjectb = fastmatch::geteasyobjectb();
          imatchobjectw = fastmatch::geteasyobjectw();
          int iobj = 1;
          if (m_icompareobject == 0) {
            iobj = 1;
          } else if (m_icompareobject == 1) {
            iobj = 0;
            if (imatchobjectb == m_imodelobjectb &&
                imatchobjectw == m_imodelobjectw) {
              iobj = 1;
            }
          } else if (m_icompareobject == 2) {
            iobj = 0;
            if (imatchobjectb == m_imodelobjectb) {
              iobj = 1;
            }
          } else if (m_icompareobject == 3) {
            iobj = 0;

            if (imatchobjectw == m_imodelobjectw) {
              iobj = 1;
            }
          }

          if (dimagevalue > dmaxvalue && iobj > 0) {
            dmaxvalue = dimagevalue;
            iresultfont = iselnum;
            imaxnum = iselnum;
            iselmaxnum = i;
          }
          if (-1 == m_idebugfontnum && 100 == dimagevalue) {
            break;
          }
        }
        if (iselnum == m_idebugfontnum)
          break;
      }

      if (0 == ilevel) {
        m_l3resultlist.push_back(imaxnum);
      } else if (1 == ilevel) {
        m_l6resultlist.push_back(iresultfont);
      } else if (2 == ilevel) {
        if (-1 == m_idebugrectsnum && -1 == m_idebugfontnum) {
          QRegExp rxnum("(\\d+)");
          std::StringList listother = m_fontlist_l12[iresultfont].split(rxnum);

          QRegExp rxother("(\\D+)");
          std::StringList listnum = m_fontlist_l12[iresultfont].split(rxother);

          std::String strkey = m_fontlist_l12[iresultfont];
          if (listother.size() > 0) {
            if (listother[0] != "")
              strkey = listother[0];
            if (listnum.size() < 2) {
              strkey = listnum[0].mid(0, 1);
            }
          }
          m_resultstrlist.push_back(strkey);
          m_resultstring.append(strkey);
        } else {
          std::String strkey = m_fontlist_l12[iresultfont];
          m_resultstrlist.push_back(strkey);
          m_resultstring.append(strkey);
        }

        m_l12resultlist.push_back(iresultfont);
      }
    }
  }
  if (-1 != m_idebugrectsnum && -1 != m_idebugfontnum) {
    std::String astr = std::String("  %1   %2 b(%3 %5)w(%4 %6)")
                           .arg(dvalue)
                           .arg(dimagevalue)
                           .arg(imatchobjectb)
                           .arg(imatchobjectw)
                           .arg(m_imodelobjectb)
                           .arg(m_imodelobjectw);
    Shape::setname(astr.toStdString().c_str());
  } else if (-1 != m_idebugrectsnum && -1 == m_idebugfontnum) {
    int iselnum = SelectMapModel(ilevel, m_idebugrectsnum, iselmaxnum);
    fastmatch::MatchGrid(m_pimagegrid);

    imatchobjectb = fastmatch::geteasyobjectb();
    imatchobjectw = fastmatch::geteasyobjectw();
    std::String strt = std::String(" %1 ").arg(iresultfont) + m_resultstring +
                       std::String(" %1").arg(iselnum);
    strt = strt + std::String(" b(%3 %5)w(%4 %6)")
                      .arg(imatchobjectb)
                      .arg(imatchobjectw)
                      .arg(m_imodelobjectb)
                      .arg(m_imodelobjectw);
    Shape::setname(strt.toStdString().c_str());
  } else {
    if (0 == ilevel) {
      std::String qshowstr;
      int isize = m_l3resultlist.size();
      for (int i = 0; i < isize; i++) {
        std::String astrnum = std::String("%1|").arg(m_l3resultlist[i]);
        qshowstr = qshowstr + astrnum;
      }

      Shape::setname(qshowstr.toStdString().c_str());
    } else if (1 == ilevel) {
      std::String qshowstr;
      int isize = m_l6resultlist.size();
      for (int i = 0; i < isize; i++) {
        std::String astrnum = std::String("%1|").arg(m_l6resultlist[i]);
        qshowstr = qshowstr + astrnum;
      }
      Shape::setname(qshowstr.toStdString().c_str());
    } else if (2 == ilevel)
      Shape::setname(m_resultstring.toStdString().c_str());
  }
}

void EasyOCR::fontocr_level2() {

  int iareasnum = g_pbackfindobject->getresultobjsnum();
  if (m_l6resultlist.size() <= 0)
    return;
  if (m_l6resultlist.size() != iareasnum)
    return;
  m_resultstrlist.clear();
  m_resultstring.clear();

  double dvalue = 0;
  double dimagevalue = 0;
  double dmaxvalue = 0;
  int imatchobjectw = 0;
  int imatchobjectb = 0;

  int iresultfont = 0;
  int iresultnum = 0;
  if (iareasnum <= 0)
    return;

  for (int ia = 0; ia < iareasnum; ia++) {
    if (-1 == m_idebugrectsnum || ia == m_idebugrectsnum) {
      int isize = imagefastmapsize(2, ia);
      Rect arect = g_pbackfindobject->getgrid(ia);
      int ix = arect.x();
      int iy = arect.y();
      int iw = arect.width();
      int ih = arect.height();
      fastmatch::setmatchrect(ix, iy, iw, ih);
      dmaxvalue = 0;
      iresultfont = 0;
      iresultnum = 0;
      if (1 == isize) {
        int iselnum = SelectMapModel(2, ia, 0);
        dmaxvalue = 100;
        iresultfont = iselnum;
        iresultnum = 0;

      } else
        for (int i = 0; i < isize; i++) {
          int iselnum = SelectMapModel(2, ia, i);
          if (-1 == m_idebugfontnum || i == m_idebugfontnum) {

            fastmatch::Match(*g_pbackobjectimage);

            dvalue = fastmatch::getmaxresult();

            dimagevalue = 0;
            if (dvalue >= m_dmatchthre) {
              fastmatch::imagematch(-1, 1);
              dimagevalue = fastmatch::getimagemodelreslut();

              imatchobjectb = fastmatch::geteasyobjectb();
              imatchobjectw = fastmatch::geteasyobjectw();
              int iobj = 1;
              if (m_icompareobject == 0) {
                iobj = 1;
              } else if (m_icompareobject == 1) {
                iobj = 0;
                if (imatchobjectb == m_imodelobjectb &&
                    imatchobjectw == m_imodelobjectw) {
                  iobj = 1;
                }
              } else if (m_icompareobject == 2) {
                iobj = 0;
                if (imatchobjectb == m_imodelobjectb) {
                  iobj = 1;
                }
              } else if (m_icompareobject == 3) {
                iobj = 0;

                if (imatchobjectw == m_imodelobjectw) {
                  iobj = 1;
                }
              }

              if (dimagevalue > dmaxvalue && iobj > 0) {
                dmaxvalue = dimagevalue;
                iresultfont = iselnum;
                iresultnum = i;
              }
              if (-1 == m_idebugfontnum && 100 == dimagevalue)
                break;
            }
          }
        }

      if (-1 == m_idebugrectsnum && -1 == m_idebugfontnum) {
        QRegExp rxnum("(\\d+)");
        std::StringList listother = m_fontlist_l12[iresultfont].split(rxnum);

        QRegExp rxother("(\\D+)");
        std::StringList listnum = m_fontlist_l12[iresultfont].split(rxother);

        std::String strkey = m_fontlist_l12[iresultfont];
        if (listother.size() > 0) {
          if (listother[0] != "")
            strkey = listother[0];
          if (listnum.size() < 2) {
            strkey = listnum[0].mid(0, 1);
          }
        }
        m_resultstrlist.push_back(strkey);
        m_resultstring.append(strkey);

      } else {
        std::String strkey = m_fontlist_l12[iresultfont];
        m_resultstrlist.push_back(strkey);
        m_resultstring.append(strkey);
      }
    }
  }
  if (-1 != m_idebugrectsnum && -1 != m_idebugfontnum) {
    std::String astr = std::String("  %1   %2 b(%3 %5)w(%4 %6)")
                           .arg(dvalue)
                           .arg(dimagevalue)
                           .arg(imatchobjectb)
                           .arg(imatchobjectw)
                           .arg(m_imodelobjectb)
                           .arg(m_imodelobjectw);
    Shape::setname(astr.toStdString().c_str());
  } else if (-1 != m_idebugrectsnum && -1 == m_idebugfontnum) {
    int iselnum = SelectMapModel(2, m_idebugrectsnum, iresultnum);

    fastmatch::Match(*g_pbackobjectimage);

    fastmatch::imagematch(-1, 1);

    imatchobjectb = fastmatch::geteasyobjectb();
    imatchobjectw = fastmatch::geteasyobjectw();
    std::String strt = std::String(" %1 ").arg(iresultfont) + m_resultstring +
                       std::String(" %1").arg(dmaxvalue);
    strt = strt + std::String(" b(%3 %5)w(%4 %6)")
                      .arg(imatchobjectb)
                      .arg(imatchobjectw)
                      .arg(m_imodelobjectb)
                      .arg(m_imodelobjectw);
    Shape::setname(strt.toStdString().c_str());
  } else
    Shape::setname(m_resultstring.toStdString().c_str());
}
void EasyOCR::checkocr_level3() {
  int iareasnum = g_pbackfindobject->getresultobjsnum();

  m_l12resultlist.clear();

  int isize = m_reslutnodelist.size();
  for (int it = 0; it < isize; it++) {
    if (2 == m_reslutnodelist[it].s_ilevel) {
      m_l12resultlist.push_back(m_reslutnodelist[it].s_inode);
    } else {
      Shape::setname(
          std::String("node result level != 2").toStdString().c_str());
      return;
    }
  }

  m_resultstrlist.clear();
  m_resultstring.clear();

  double dvalue = 0;
  double dimagevalue = 0;
  double dmaxvalue = 0;
  int imatchobjectw = 0;
  int imatchobjectb = 0;

  int iresultfont = 0;
  int iresultnum = 0;
  if (iareasnum <= 0)
    return;

  for (int ia = 0; ia < iareasnum; ia++) {
    if (-1 == m_idebugrectsnum || ia == m_idebugrectsnum) {
      int isize = resultnodesize(3, ia);
      Rect arect = g_pbackfindobject->getgrid(ia);
      int ix = arect.x();
      int iy = arect.y();
      int iw = arect.width();
      int ih = arect.height();
      fastmatch::setmatchrect(ix, iy, iw, ih);
      dmaxvalue = 0;
      iresultfont = 0;
      iresultnum = 0;
      for (int i = 0; i < isize; i++) {
        selectresultnode(3, ia, i);
        if (-1 == m_idebugfontnum || i == m_idebugfontnum) {
          fastmatch::Match(*g_pbackobjectimage);

          dvalue = fastmatch::getmaxresult();

          dimagevalue = 0;
          if (dvalue >= m_dmatchthre) {
            fastmatch::imagematch_grid(-1, 1, 36);
            dimagevalue = fastmatch::getimagemodelreslut();

            if (dimagevalue > dmaxvalue) {
              dmaxvalue = dimagevalue;

              m_iresultfont = m_selectnode.s_inode;
              iresultfont = m_selectnode.s_inode;
              iresultnum = i;
            }
            if (-1 == m_idebugfontnum && 100 == dimagevalue)
              break;
          }
        }
      }

      if (-1 == m_idebugrectsnum && -1 == m_idebugfontnum) {
        QRegExp rxnum("(\\d+)");
        std::StringList listother = m_fontlist_l36[m_iresultfont].split(rxnum);

        QRegExp rxother("(\\D+)");
        std::StringList listnum = m_fontlist_l36[m_iresultfont].split(rxother);

        std::String strkey = m_fontlist_l36[m_iresultfont];
        if (listother.size() > 0) {
          if (listother[0] != "")
            strkey = listother[0];
          if (listnum.size() < 2) {
            strkey = listnum[0].mid(0, 1);
          }
        }
        std::String astr =
            std::String("  %1   %2 ").arg(dvalue).arg(dimagevalue);
        m_resultstrlist.push_back(strkey + astr);
        m_resultstring.append(strkey + astr);

      } else {
        std::String astr =
            std::String("  %1   %2 ").arg(dvalue).arg(dimagevalue);

        std::String strkey = m_filenamelist_l36[iresultfont];
        m_resultstrlist.push_back(strkey + astr);
        m_resultstring.append(strkey + astr);
      }
    }
  }
  if (-1 != m_idebugrectsnum && -1 != m_idebugfontnum) {
    std::String astr = std::String("  %1   %2 ").arg(dvalue).arg(dimagevalue);
    Shape::setname(astr.toStdString().c_str());
  } else if (-1 != m_idebugrectsnum && -1 == m_idebugfontnum) {
    selectresultnode(3, m_idebugrectsnum, iresultnum);

    fastmatch::Match(*g_pbackobjectimage);

    fastmatch::imagematch_grid(-1, 1, 36);

    std::String strt = std::String(" %1 ").arg(iresultfont) + m_resultstring +
                       std::String(" %1").arg(dmaxvalue);

    Shape::setname(strt.toStdString().c_str());
  } else
    Shape::setname(m_resultstring.toStdString().c_str());
}

bool EasyOCR::matchlevelnode01() {
  MatchGrid(m_pimagegrid);
  double dimagevalue = fastmatch::getimagemodelreslut();

  int imatchobjectb = fastmatch::geteasyobjectb();
  int imatchobjectw = fastmatch::geteasyobjectw();
  int iobj = 1;
  if (m_icompareobject == 0) {
    iobj = 1;
  } else if (m_icompareobject == 1) {
    iobj = 0;
    if (imatchobjectb == m_imodelobjectb && imatchobjectw == m_imodelobjectw) {
      iobj = 1;
    }
  } else if (m_icompareobject == 2) {
    iobj = 0;
    if (imatchobjectb == m_imodelobjectb) {
      iobj = 1;
    }
  } else if (m_icompareobject == 3) {
    iobj = 0;

    if (imatchobjectw == m_imodelobjectw) {
      iobj = 1;
    }
  }

  if (dimagevalue > m_dmaxvalue && iobj > 0) {
    m_iselmaxnum = m_selectnode.s_inode;
    m_dmaxvalue = dimagevalue;
    m_iresultfont = m_selectnode.s_inode;
    m_imaxnum = m_selectnode.s_inode;
  }
  if (-1 == m_idebugfontnum && 100 == dimagevalue) {
    return false;
  }

  return true;
}
bool EasyOCR::matchlevelnode2() {
  fastmatch::Match(*g_pbackimage);

  double dvalue = fastmatch::getmaxresult();
  double dimagevalue = 0;
  if (dvalue >= m_dmatchthre) {
    fastmatch::imagematch(-1, 1, 12, 0);
    dimagevalue = fastmatch::getimagemodelreslut();
    int imatchobjectb = fastmatch::geteasyobjectb();
    int imatchobjectw = fastmatch::geteasyobjectw();
    int iobj = 1;
    if (m_icompareobject == 0) {
      iobj = 1;
    } else if (m_icompareobject == 1) {
      iobj = 0;
      if (imatchobjectb == m_imodelobjectb &&
          imatchobjectw == m_imodelobjectw) {
        iobj = 1;
      }
    } else if (m_icompareobject == 2) {
      iobj = 0;
      if (imatchobjectb == m_imodelobjectb) {
        iobj = 1;
      }
    } else if (m_icompareobject == 3) {
      iobj = 0;

      if (imatchobjectw == m_imodelobjectw) {
        iobj = 1;
      }
    }
    if (dimagevalue > m_dmaxvalue) {
      if (iobj > 0) {
        m_dvalue = dvalue;
        m_dmaxvalue = dimagevalue;
        m_iresultfont = m_selectnode.s_inode;
        m_imaxnum = m_selectnode.s_inode;
      } else if (m_dmaxvalue <= 0) {
        m_dvalue = dvalue;
        m_dmaxvalue = dimagevalue;
        m_iresultfont = m_selectnode.s_inode;
        m_imaxnum = m_selectnode.s_inode;
      }
    }
    if (-1 == m_idebugfontnum && 100 == dimagevalue) {
      return false;
    }

    return true;
  } else
    return true;
}
void EasyOCR::resultnodelistreset(int iareanum) {
  m_reslutnodeslistgrid3x3.clear();
  m_reslutnodeslistgrid6x6.clear();
  m_reslutnodeslistgrid12x12.clear();
  m_reslutnodeslistgrid36x36.clear();
  m_reslutnodeslistgrid72x72.clear();

  for (int ia = 0; ia < iareanum; ia++) {
    levelnodes anodes;
    anodes.setsearchnum(m_resultnodesearchsum);
    m_reslutnodeslistgrid3x3.push_back(anodes);
    m_reslutnodeslistgrid6x6.push_back(anodes);
    m_reslutnodeslistgrid12x12.push_back(anodes);
  }
}
bool EasyOCR::matchlevelnodelist3x3(int ia) {
  int isize = imagefastmodelsize(0);
  double dimagevalue = 0;
  for (int i = 0; i < isize; i++) {
    fastmatch::modelstocurrent_l3(i);
    fastmatch::imagemodelstocurrent_l3(i);
    MatchGrid(m_pimagegrid);
    dimagevalue = fastmatch::getimagemodelreslut();

    levelvalenode anode;
    anode.s_ilevel = 0;
    anode.s_inode = i;
    anode.s_dvalue = dimagevalue;

    m_reslutnodeslistgrid3x3[ia].addnode(anode);
  }

  return true;
}
bool EasyOCR::matchlevelnodelist6x6(int ia) {
  int isize = m_reslutnodeslistgrid3x3[ia].getnodes().size();
  double dimagevalue = 0;
  for (int i = 0; i < isize; i++) {
    int inodenum = m_reslutnodeslistgrid3x3[ia].getnodes()[i].s_inode;

    for (int j = 0; j < getlevel3_6map().size(); j++) {
      if (getlevel3_6map()[j] == inodenum) {
        fastmatch::modelstocurrent_l6(j);
        fastmatch::imagemodelstocurrent_l6(j);
        MatchGrid(m_pimagegrid);
        dimagevalue = fastmatch::getimagemodelreslut();
        levelvalenode anode;
        anode.s_ilevel = 1;
        anode.s_inode = j;
        anode.s_dvalue = dimagevalue;

        m_reslutnodeslistgrid6x6[ia].addnode(anode);
      }
    }
  }

  return true;
}
bool EasyOCR::matchlevelnodelist12x12(int ia) {
  int isize = m_reslutnodeslistgrid6x6[ia].getnodes().size();
  double dimagevalue = 0;
  for (int i = 0; i < isize; i++) {
    int inodenum = m_reslutnodeslistgrid6x6[ia].getnodes()[i].s_inode;

    for (int j = 0; j < getlevel6_12map().size(); j++) {
      if (getlevel6_12map()[j] == inodenum) {
        fastmatch::modelstocurrent_l12(j);
        fastmatch::imagemodelstocurrent_l12(j);
        fastmatch::Match(*g_pbackimage);
        double dvalue = fastmatch::getmaxresult();
        double dimagevalue = 0;
        if (dvalue >= m_dmatchthre) {
          fastmatch::imagematch(-1, 1, 12, 0);
          dimagevalue = fastmatch::getimagemodelreslut();

          levelvalenode anode;
          anode.s_ilevel = 2;
          anode.s_inode = j;

          anode.s_dvalue = dimagevalue;
          m_reslutnodeslistgrid12x12[ia].addnode(anode);
        }
      }
    }
  }

  return true;
}
bool EasyOCR::matchlevelnodelist36x36(int ia) {
  fastmatch::Match(*g_pbackimage);
  double dvalue = fastmatch::getmaxresult();
  double dimagevalue = 0;
  if (dvalue >= m_dmatchthre) {
    fastmatch::imagematch(-1, 1, 12, 0);
    dimagevalue = fastmatch::getimagemodelreslut();
    if (dimagevalue > m_dmaxvalue) {
      m_dvalue = dvalue;
      m_dmaxvalue = dimagevalue;
      m_iresultfont = m_selectnode.s_inode;
      m_imaxnum = m_selectnode.s_inode;
    }
    if (-1 == m_idebugfontnum && 100 == dimagevalue) {
      return false;
    }
    return true;
  } else
    return true;
}

void EasyOCR::fontocr_levelnode(int ilevel) {
  int iareasnum = g_pbackfindobject->getresultobjsnum();
  m_resultstrlist.clear();
  m_resultstring.clear();
  if (iareasnum <= 1)
    return;
  m_ilevle = ilevel;

  m_dvalue = 0;
  m_dmaxvalue = 0;
  if (0 == ilevel) {
    resultnodereset(iareasnum);
  }
  int imatchobjectw = 0;
  int imatchobjectb = 0;

  for (int ia = 0; ia < iareasnum; ia++) {
    if (-1 == m_idebugrectsnum || ia == m_idebugrectsnum) {
      int isize = resultnodesize(ilevel, ia);

      Rect arect = g_pbackfindobject->getgridex(ia);

      int iobjw = g_pbackfindobject->getresultw(ia);
      int iobjh = g_pbackfindobject->getresulth(ia);
      int imaxlen = iobjw > iobjh ? iobjw : iobjh;
      int igrid_org = fastmatch::GetRectGridLevel(imaxlen);

      int ix = arect.x();
      int iy = arect.y();
      int iw = arect.width();
      int ih = arect.height();

      if (ix < 0 || iy < 0 || iw <= 0 || ih <= 0)
        continue;

      switch (ilevel) {
      case 0:
        fastmatch::setmatchrect(ix, iy, iw, ih);
        g_pbackobjectimage->SetROI(ix, iy, iw, ih);
        g_pbackimage->SetROI(ix, iy, iw, ih);
        g_pbackobjectimage->SetMode(3);
        g_pbackobjectimage->ROItoROI(g_pbackimage);

        g_pbackimage->ROIColorTable();
        g_pbackimage->ROIColorTableBlur(0, -1);
        g_pbackimage->ROIColorTableEasyThre(1);

        m_pimagegrid->ROIImagetoModel(*g_pbackimage);
        m_pimagegrid->ZeroModel();
        m_pimagegrid->ReGrid(igrid_org, igrid_org);

        if (0) {
          m_pimagegrid->SetUnit(igrid_org, igrid_org);
          m_pimagegrid->UnitGrid();
        }
        m_pimagegrid->GridZoom(3, 3);
        m_pimagegrid->SetUnit(3, 3);

        break;
      case 1:
        fastmatch::setmatchrect(ix, iy, iw, ih);
        g_pbackimage->SetROI(ix, iy, iw, ih);
        m_pimagegrid->ROIImagetoModel(*g_pbackimage);
        m_pimagegrid->ZeroModel();
        m_pimagegrid->ReGrid(igrid_org, igrid_org);

        if (0) {
          m_pimagegrid->SetUnit(igrid_org, igrid_org);
          m_pimagegrid->UnitGrid();
        }
        m_pimagegrid->GridZoom(6, 6);
        m_pimagegrid->SetUnit(6, 6);
        break;
      default:
      case 2:
        if (igrid_org == 12)
          fastmatch::setmatchrect(ix, iy, iw, ih);
        else {
          g_pbackimage->SetROI(ix, iy, iw, ih);
          m_pimagegrid->ROIImagetoModel(*g_pbackimage);

          m_pimagegrid->ZeroModel();
          m_pimagegrid->ReGrid(igrid_org, igrid_org);
          if (0) {
            m_pimagegrid->SetUnit(igrid_org, igrid_org);
            m_pimagegrid->UnitGrid();
          }

          m_pimagegrid->GridZoom(12, 12);
          m_pimagegrid->SetUnit(12, 12);
        }
        break;
      case 3:
        fastmatch::setmatchrect(ix, iy, iw, ih);

        break;
      }

      m_dmaxvalue = 0;
      m_iselmaxnum = 0;
      m_iresultfont = 0;
      for (int i = 0; i < isize; i++) {
        if (-1 == m_idebugfontnum)
          selectresultnode(ilevel, ia, i);
        else
          SelectModel(ilevel, m_idebugfontnum);
        if (1) {
          if (0 == ilevel || 1 == ilevel) {
            if (!matchlevelnode01()) {
              setresultnode(ia, m_selectnode);
              break;
            }
          } else if (2 == ilevel) {
            if (1 == isize && -1 == m_idebugfontnum) {
              m_dvalue = 1;
              m_dmaxvalue = 99;
              m_iresultfont = m_selectnode.s_inode;
              m_imaxnum = m_selectnode.s_inode;
              break;
            } else if (igrid_org != 12) {
              if (!matchlevelnode01()) {
                setresultnode(ia, m_selectnode);
                break;
              }
            } else if (!matchlevelnode2())
              break;
          }
        }

        if (m_selectnode.s_inode == m_idebugfontnum)
          break;
      }
      if (1 == ilevel || 0 == ilevel) {
        if (m_reslutnodelist[ia].s_inode == -1) {
          if (m_reslutnodelist[ia].s_ilevel == 0) {
            if (ilevel == 1) {
              m_reslutnodelist[ia].s_inode = -1;
              m_reslutnodelist[ia].s_ilevel = 1;
            }
          } else if (m_reslutnodelist[ia].s_ilevel == 1) {
            if (ilevel == 2) {
              m_reslutnodelist[ia].s_inode = -1;
              m_reslutnodelist[ia].s_ilevel = 2;
            }
          }
        }
      }
      if (2 == ilevel) {
        if (-1 == m_idebugrectsnum && -1 == m_idebugfontnum) {

          m_reslutnodelist[ia].s_inode = m_iresultfont;
          m_reslutnodelist[ia].s_ilevel = 2;
          int iresultfont = m_iresultfont;
          QRegExp rxnum("(\\d+)");
          std::StringList listother = m_fontlist_l12[iresultfont].split(rxnum);

          QRegExp rxother("(\\D+)");
          std::StringList listnum = m_fontlist_l12[iresultfont].split(rxother);

          std::String strkey = m_fontlist_l12[iresultfont];
          if (listother.size() > 0) {
            if (listother[0] != "")
              strkey = listother[0];
            if (listnum.size() < 2) {
              strkey = listnum[0].mid(0, 1);
            }
          }
          m_resultstrlist.push_back(strkey);
          m_resultstring.append(strkey);
        } else {
          m_reslutnodelist[ia].s_inode = m_iresultfont;
          m_reslutnodelist[ia].s_ilevel = 2;
          int iresultfont = m_iresultfont;
          std::String strkey = m_fontlist_l12[iresultfont];
          m_resultstrlist.push_back(strkey);
          m_resultstring.append(strkey);
        }
      }
    }
  }
  if (-1 != m_idebugrectsnum && -1 != m_idebugfontnum) {

    imatchobjectb = fastmatch::geteasyobjectb();
    imatchobjectw = fastmatch::geteasyobjectw();
    std::String astr = std::String("  %1   %2 b(%3 %5)w(%4 %6)")
                           .arg(m_dvalue)
                           .arg(m_dmaxvalue)
                           .arg(imatchobjectb)
                           .arg(imatchobjectw)
                           .arg(m_imodelobjectb)
                           .arg(m_imodelobjectw);
    Shape::setname(astr.toStdString().c_str());
  } else if (-1 != m_idebugrectsnum && -1 == m_idebugfontnum) {
    if (m_idebugrectsnum >= iareasnum)
      return;
    selectresultnode(ilevel, m_idebugrectsnum, m_iselmaxnum);
    if (0 == ilevel || 1 == ilevel)
      fastmatch::MatchGrid(m_pimagegrid);
    else if (2 == ilevel) {
      fastmatch::Match(*g_pbackobjectimage);
      m_dvalue = fastmatch::getmaxresult();
      fastmatch::imagematch(-1, 1);
    }
    imatchobjectb = fastmatch::geteasyobjectb();
    imatchobjectw = fastmatch::geteasyobjectw();
    std::String strt = std::String(" %1 %2 ").arg(m_dvalue).arg(m_dmaxvalue) +
                       m_resultstring +
                       std::String(" %1").arg(m_selectnode.s_inode);
    strt = strt + std::String(" b(%3 %5)w(%4 %6) ")
                      .arg(imatchobjectb)
                      .arg(imatchobjectw)
                      .arg(m_imodelobjectb)
                      .arg(m_imodelobjectw);
    if (2 == ilevel)
      strt = strt + m_fontlist_l12[m_iresultfont];
    Shape::setname(strt.toStdString().c_str());
  } else {
    std::String qshowstr;
    int isize = m_reslutnodelist.size();
    for (int it = 0; it < isize; it++) {
      std::String astrnum = std::String("%3_%2(%1)")
                                .arg(m_reslutnodelist[it].s_ilevel)
                                .arg(m_reslutnodelist[it].s_inode)
                                .arg(it);
      qshowstr = qshowstr + astrnum;
    }
    Shape::setname(qshowstr.toStdString().c_str());
  }
}

void EasyOCR::fontocr_levelnodelist() {
  int iareasnum = g_pbackfindobject->getresultobjsnum();
  m_resultstrlist.clear();
  m_resultstring.clear();
  if (iareasnum <= 0)
    return;

  m_dvalue = 0;
  m_dmaxvalue = 0;
  resultnodelistreset(iareasnum);

  int imatchobjectw = 0;
  int imatchobjectb = 0;

  for (int ia = 0; ia < iareasnum; ia++) {
    if (-1 == m_idebugrectsnum || ia == m_idebugrectsnum) {
      Rect arect = g_pbackfindobject->getgridex(ia);

      int iobjw = g_pbackfindobject->getresultw(ia);
      int iobjh = g_pbackfindobject->getresulth(ia);
      int imaxlen = iobjw > iobjh ? iobjw : iobjh;
      int igrid_org = fastmatch::GetRectGridLevel(imaxlen);

      int ix = arect.x();
      int iy = arect.y();
      int iw = arect.width();
      int ih = arect.height();

      if (ix < 0 || iy < 0 || iw <= 0 || ih <= 0)
        continue;

      fastmatch::setmatchrect(ix, iy, iw, ih);
      g_pbackobjectimage->SetROI(ix, iy, iw, ih);
      g_pbackimage->SetROI(ix, iy, iw, ih);
      g_pbackobjectimage->SetMode(3);
      g_pbackobjectimage->ROItoROI(g_pbackimage);

      g_pbackimage->ROIColorTable();
      g_pbackimage->ROIColorTableBlur(0, -1);
      g_pbackimage->ROIColorTableEasyThre(1);

      m_pimagegrid->ROIImagetoModel(*g_pbackimage);
      m_pimagegrid->ZeroModel();
      m_pimagegrid->ReGrid(igrid_org, igrid_org);

      m_pimagegrid->GridZoom(3, 3);
      m_pimagegrid->SetUnit(3, 3);

      matchlevelnodelist3x3(ia);

      fastmatch::setmatchrect(ix, iy, iw, ih);
      g_pbackimage->SetROI(ix, iy, iw, ih);
      m_pimagegrid->ROIImagetoModel(*g_pbackimage);
      m_pimagegrid->ZeroModel();
      m_pimagegrid->ReGrid(igrid_org, igrid_org);

      m_pimagegrid->GridZoom(6, 6);
      m_pimagegrid->SetUnit(6, 6);

      matchlevelnodelist6x6(ia);
      fastmatch::setmatchrect(ix, iy, iw, ih);
      if (0) {
        g_pbackimage->SetROI(ix, iy, iw, ih);
        m_pimagegrid->ROIImagetoModel(*g_pbackimage);

        m_pimagegrid->ZeroModel();
        m_pimagegrid->ReGrid(igrid_org, igrid_org);

        m_pimagegrid->GridZoom(12, 12);
        m_pimagegrid->SetUnit(12, 12);
      }
      matchlevelnodelist12x12(ia);

      m_dmaxvalue = 0;
      m_iselmaxnum = 0;
      m_iresultfont = 0;

      if (0) {
        if (-1 == m_idebugrectsnum && -1 == m_idebugfontnum) {

          m_reslutnodelist[ia].s_inode = m_iresultfont;
          m_reslutnodelist[ia].s_ilevel = 2;
          int iresultfont = m_iresultfont;
          QRegExp rxnum("(\\d+)");
          std::StringList listother = m_fontlist_l12[iresultfont].split(rxnum);

          QRegExp rxother("(\\D+)");
          std::StringList listnum = m_fontlist_l12[iresultfont].split(rxother);

          std::String strkey = m_fontlist_l12[iresultfont];
          if (listother.size() > 0) {
            if (listother[0] != "")
              strkey = listother[0];
            if (listnum.size() < 2) {
              strkey = listnum[0].mid(0, 1);
            }
          }
          m_resultstrlist.push_back(strkey);
          m_resultstring.append(strkey);
        } else {
          m_reslutnodelist[ia].s_inode = m_iresultfont;
          m_reslutnodelist[ia].s_ilevel = 2;
          int iresultfont = m_iresultfont;
          std::String strkey = m_fontlist_l12[iresultfont];
          m_resultstrlist.push_back(strkey);
          m_resultstring.append(strkey);
        }
      }
    }
  }
  if (0) {
    if (-1 != m_idebugrectsnum && -1 != m_idebugfontnum) {

      imatchobjectb = fastmatch::geteasyobjectb();
      imatchobjectw = fastmatch::geteasyobjectw();
      std::String astr = std::String("  %1   %2 b(%3 %5)w(%4 %6)")
                             .arg(m_dvalue)
                             .arg(m_dmaxvalue)
                             .arg(imatchobjectb)
                             .arg(imatchobjectw)
                             .arg(m_imodelobjectb)
                             .arg(m_imodelobjectw);
      Shape::setname(astr.toStdString().c_str());
    } else if (-1 != m_idebugrectsnum && -1 == m_idebugfontnum) {
      if (m_idebugrectsnum >= iareasnum)
        return;
      fastmatch::MatchGrid(m_pimagegrid);
      {
        fastmatch::Match(*g_pbackobjectimage);
        m_dvalue = fastmatch::getmaxresult();
        fastmatch::imagematch(-1, 1);
      }
      imatchobjectb = fastmatch::geteasyobjectb();
      imatchobjectw = fastmatch::geteasyobjectw();
      std::String strt = std::String(" %1 %2 ").arg(m_dvalue).arg(m_dmaxvalue) +
                         m_resultstring +
                         std::String(" %1").arg(m_selectnode.s_inode);
      strt = strt + std::String(" b(%3 %5)w(%4 %6) ")
                        .arg(imatchobjectb)
                        .arg(imatchobjectw)
                        .arg(m_imodelobjectb)
                        .arg(m_imodelobjectw);
      strt = strt + m_fontlist_l12[m_iresultfont];
      Shape::setname(strt.toStdString().c_str());
    } else {
      std::String qshowstr;
      int isize = m_reslutnodelist.size();
      for (int it = 0; it < isize; it++) {
        std::String astrnum = std::String("%3_%2(%1)")
                                  .arg(m_reslutnodelist[it].s_ilevel)
                                  .arg(m_reslutnodelist[it].s_inode)
                                  .arg(it);
        qshowstr = qshowstr + astrnum;
      }
      Shape::setname(qshowstr.toStdString().c_str());
    }
  }
}

void EasyOCR::shownoderesult() {
  std::String qshowstr;
  int isize = m_reslutnodelist.size();
  std::String strpatA("");
  std::String strpatB("");

  std::String strpata("");
  std::String strpatb("");
  std::String strpatc("");

  for (int i = 0; i < isize; i++) {
    int iresultfont = m_reslutnodelist[i].s_inode;
    if (iresultfont < 0)
      continue;

    QRegExp rxnum("(\\d+)");

    std::StringList listother = m_fontlist_l12[iresultfont].split(rxnum);

    QRegExp rxother("(\\D+)");
    std::StringList listnum = m_fontlist_l12[iresultfont].split(rxother);
    std::String strkey = m_fontlist_l12[iresultfont];
    if (listother.size() > 0) {
      if (listother[0] != "")
        strkey = listother[0];
      if (listnum.size() < 2) {
        strkey = listnum[0].mid(0, 1);
      }
      std::StringList strpat = strkey.split("*");
      if (strpat.size() > 1) {
        if ((!strpat[0].isEmpty()) || (!strpat[1].isEmpty())) {
          if (strpatA.isEmpty()) {
            strpatA = strpat[0];
            strkey = std::String("");
          } else if (!strpatA.isEmpty()) {
            strpatB = strpat[1];
            { strkey = strpatB; }
            strpatA = std::String("");
            strpatB = std::String("");
          }
        }
      }
      strpat = strkey.split("|");
      if (strpat.size() > 1) {
        if ((!strpat[0].isEmpty()) || (!strpat[1].isEmpty())) {
          if (strpata.isEmpty()) {
            strpata = strpat[0];
            strkey = std::String("");
          } else if ((!strpata.isEmpty()) && strpatb.isEmpty()) {
            strpatb = strpat[1];
            strkey = std::String("");
          } else if ((!strpata.isEmpty()) && (!strpatb.isEmpty()) &&
                     (strpatc.isEmpty())) {
            strpatc = strpat[1];
            { strkey = strpatc; }
            strpata = std::String("");
            strpatb = std::String("");
            strpatc = std::String("");
          }
        }
      }
    }
    qshowstr = qshowstr + strkey;
  }

  Shape::setname(qshowstr.toStdString().c_str());
}
void EasyOCR::shownoderesultex() {
  std::String qshowstr;
  int isize = m_reslutnodelist.size();
  std::String strpatA("");
  std::String strpatB("");

  std::String strpata("");
  std::String strpatb("");
  std::String strpatc("");

  std::StringList strlista;
  std::StringList strlistb;
  std::StringList strlistc;

  int il12size = getduplicateslist_l12().size();

  for (int i = 0; i < isize; i++) {
    int iresultfont = m_reslutnodelist[i].s_inode;
    if (iresultfont < 0)
      continue;
    int igetvalue = getduplicateslist_l12()[iresultfont];
    if (igetvalue != 0) {
      strlista.clear();
      for (int j = 0; j < il12size; j++) {
        if (igetvalue == getduplicateslist_l12()[j]) {
          std::String strget = ABC2string(m_fontlist_l12[j]);
          if (!strget.isEmpty())
            strlista.push_back(strget);
        }
      }
    }

    QRegExp rxnum("(\\d+)");
    std::StringList listother = m_fontlist_l12[iresultfont].split(rxnum);

    QRegExp rxother("(\\D+)");
    std::StringList listnum = m_fontlist_l12[iresultfont].split(rxother);
    std::String strkey = m_fontlist_l12[iresultfont];
    if (listother.size() > 0) {
      if (listother[0] != "")
        strkey = listother[0];
      if (listnum.size() < 2) {
        strkey = listnum[0].mid(0, 1);
      }
      std::StringList strpat = strkey.split("*");
      if (strpat.size() > 1) {
        if ((!strpat[0].isEmpty()) || (!strpat[1].isEmpty())) {
          if (strpatA.isEmpty()) {
            strpatA = strpat[0];
            strkey = std::String("");
          } else if (!strpatA.isEmpty()) {
            strpatB = strpat[1];
            {
              if (strlista.size() != 0 && strlistb.size() == 0) {
                strkey = strpatA;
              } else if (strlista.size() == 0 && strlistb.size() != 0) {
                strkey = strpatB;
              } else if (strlista.size() != 0 && strlistb.size() != 0) {
                for (int ia = 0; ia < strlista.size(); ia++) {
                  for (int ib = 0; ib < strlistb.size(); ib++) {
                    if (strlista[ia] == strlistb[ib]) {
                      strkey = strlista[ia];
                      goto ENDIAIBLOOP;
                    }
                  }
                }
              ENDIAIBLOOP:
                strlista.clear();
                strlistb.clear();

              } else
                strkey = strpatB;
            }
            strpatA = std::String("");
            strpatB = std::String("");
            strlista.clear();
            strlistb.clear();
          }
        }
      }
      strpat = strkey.split("|");
      if (strpat.size() > 1) {
        if ((!strpat[0].isEmpty()) || (!strpat[1].isEmpty())) {
          if (strpata.isEmpty()) {
            strpata = strpat[0];
            strkey = std::String("");
          } else if ((!strpata.isEmpty()) && strpatb.isEmpty()) {
            strpatb = strpat[1];
            strkey = std::String("");
          } else if ((!strpata.isEmpty()) && (!strpatb.isEmpty()) &&
                     (strpatc.isEmpty())) {
            strpatc = strpat[1];
            { strkey = strpatc; }
            strpata = std::String("");
            strpatb = std::String("");
            strpatc = std::String("");
            strlista.clear();
            strlistb.clear();
            strlistc.clear();
          }
        }
      }
    }
    qshowstr = qshowstr + strkey;

    strlistb = strlista;
    strlista.clear();
  }

  m_qocrstring = qshowstr;
  Shape::setname(qshowstr.toStdString().c_str());
}
void EasyOCR::shownodelistresult12x12() {

  std::String qshowstr;
  int isize = m_reslutnodeslistgrid12x12.size();
  std::String strpatA("");
  std::String strpatB("");

  std::String strpata("");
  std::String strpatb("");
  std::String strpatc("");

  std::StringList strlista;
  std::StringList strlistb;
  std::StringList strlistc;

  int il12size = getduplicateslist_l12().size();

  for (int i = 0; i < isize; i++) {
    if (m_idebugrectsnum == -1 || m_idebugrectsnum == i) {
      int inodesize = m_reslutnodeslistgrid12x12[i].getnodes().size();
      if (inodesize > 0) {
        int iresultfont = m_reslutnodeslistgrid12x12[i].getnodes()[0].s_inode;
        if (iresultfont < 0)
          continue;
        int ilistsize = getduplicateslist_l12().size();
        if (ilistsize > iresultfont) {

          int igetvalue = getduplicateslist_l12()[iresultfont];
          if (igetvalue != 0) {
            strlista.clear();
            for (int j = 0; j < il12size; j++) {
              if (igetvalue == getduplicateslist_l12()[j]) {
                std::String strget = ABC2string(m_fontlist_l12[j]);
                if (!strget.isEmpty())
                  strlista.push_back(strget);
              }
            }
          }

          QRegExp rxnum("(\\d+)");
          std::StringList listother = m_fontlist_l12[iresultfont].split(rxnum);

          QRegExp rxother("(\\D+)");
          std::StringList listnum = m_fontlist_l12[iresultfont].split(rxother);
          std::String strkey = m_fontlist_l12[iresultfont];
          if (listother.size() > 0) {
            if (listother[0] != "")
              strkey = listother[0];
            if (listnum.size() < 2) {
              strkey = listnum[0].mid(0, 1);
            }
            std::StringList strpat = strkey.split("*");
            if (strpat.size() > 1) {
              if ((!strpat[0].isEmpty()) || (!strpat[1].isEmpty())) {
                if (strpatA.isEmpty()) {
                  strpatA = strpat[0];
                  strkey = std::String("");
                } else if (!strpatA.isEmpty()) {
                  strpatB = strpat[1];
                  {
                    if (strlista.size() != 0 && strlistb.size() == 0) {
                      strkey = strpatA;
                    } else if (strlista.size() == 0 && strlistb.size() != 0) {
                      strkey = strpatB;
                    } else if (strlista.size() != 0 && strlistb.size() != 0) {
                      for (int ia = 0; ia < strlista.size(); ia++) {
                        for (int ib = 0; ib < strlistb.size(); ib++) {
                          if (strlista[ia] == strlistb[ib]) {
                            strkey = strlista[ia];
                            goto ENDIAIBLOOP;
                          }
                        }
                      }
                    ENDIAIBLOOP:
                      strlista.clear();
                      strlistb.clear();

                    } else
                      strkey = strpatB;
                  }
                  strpatA = std::String("");
                  strpatB = std::String("");
                  strlista.clear();
                  strlistb.clear();
                }
              }
            }
            strpat = strkey.split("|");
            if (strpat.size() > 1) {
              if ((!strpat[0].isEmpty()) || (!strpat[1].isEmpty())) {
                if (strpata.isEmpty()) {
                  strpata = strpat[0];
                  strkey = std::String("");
                } else if ((!strpata.isEmpty()) && strpatb.isEmpty()) {
                  strpatb = strpat[1];
                  strkey = std::String("");
                } else if ((!strpata.isEmpty()) && (!strpatb.isEmpty()) &&
                           (strpatc.isEmpty())) {
                  strpatc = strpat[1];
                  { strkey = strpatc; }
                  strpata = std::String("");
                  strpatb = std::String("");
                  strpatc = std::String("");
                  strlista.clear();
                  strlistb.clear();
                  strlistc.clear();
                }
              }
            }
          }
          qshowstr = qshowstr + strkey;

          strlistb = strlista;
          strlista.clear();
        } else {
          qshowstr = qshowstr + std::String("?");
        }

      } else {
        qshowstr = qshowstr + std::String("?");
      }
    }
  }

  m_qocrstring = qshowstr;
  Shape::setname(qshowstr.toStdString().c_str());
}

std::String EasyOCR::ABC2string(std::String strget) {
  std::String strpatA;
  std::String strpatB;

  std::String strpata;
  std::String strpatb;
  std::String strpatc;

  QRegExp rxnum("(\\d+)");
  std::StringList listother = strget.split(rxnum);

  QRegExp rxother("(\\D+)");
  std::StringList listnum = strget.split(rxother);
  std::String strkey = strget;

  if (listother.size() > 0) {
    if (listother[0] != "")
      strkey = listother[0];
    if (listnum.size() < 2) {
      strkey = listnum[0].mid(0, 1);
    }
    std::StringList strpat = strkey.split("*");
    if (strpat.size() > 1) {
      if (!strpat[0].isEmpty()) {
        return strpat[0];
      } else if (!strpat[1].isEmpty()) {
        return strpat[1];
      }
    }
    strpat = strkey.split("|");
    if (strpat.size() > 1) {
      if (!strpat[0].isEmpty()) {
        return strpat[0];
      } else if (!strpat[1].isEmpty()) {
        return strpat[1];
      }
    }
  }
  return std::String("");
}
std::String EasyOCR::getreslultstring() { return m_qocrstring; }
void EasyOCR::stringresulthead(const char *pchar) {
  m_qocrstring = std::String(pchar) + m_qocrstring;
}
void EasyOCR::stringresulttail(const char *pchar) {
  m_qocrstring = m_qocrstring + std::String(pchar);
}

void EasyOCR::clipboardresult() {
  QApplication::clipboard()->setText(m_qocrstring);
}

void EasyOCR::setminscore(double dminscore) {
  fastmatch::setminscore(dminscore);
}
void EasyOCR::shapesetroi(void *pshape) { Shape::shapesetroi(pshape); }
